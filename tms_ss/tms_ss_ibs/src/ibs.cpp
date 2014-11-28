//------------------------------------------------------------------------------
// @file   : ibs.cpp
// @brief  : Intelligent Board System
// @author : Akio Shigekane, Yoonseok Pyo
// @version: Ver1.1.1 (since 2012.00.00)
// @date   : 2014.11.24
//------------------------------------------------------------------------------
#include <vector> 
#include <algorithm>
#include <functional>
#include <string>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <iterator>
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <ros/ros.h>
#include <tms_msg_db/tmsdb_data.h>
#include <tms_msg_ss/ics_object_data.h>
#include <tms_msg_db/TmsdbStamped.h>
#include <tms_msg_db/Tmsdb.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdlib>

//------------------------------------------------------------------------------
// #define PORT_TR		"/dev/ttyUSB0"
// #define PORT_LC0	"/dev/ttyACM0"
//#define PORT_LC1	"/dev/ttyACM0"
std::string PORT_TR,PORT_LC0;


#define LC_MAX_SENSOR_NUM  4
#define LC_GET_WEIGHT_CNT  2
#define LC_GET_WEIGHT_STABLE 12	//

#define MAX_OBJECT_NUM		25		//環境内に存在する全物品数

//仕様上の固定値
#define TR3_STX   0x02
#define TR3_ETX   0x03
#define TR3_CR    0x0D
#define TR3_ACK   0x30
#define TR3_NACK  0x31
#define TR3_UID_SEND   0x49

#define TR3_ANT1   0x00
#define TR3_ANT2   0x01
#define TR3_ANT3   0x02
#define TR3_ANT4   0x03
#define TR3_ANT5   0x04
#define TR3_ANT6   0x05
#define TR3_ANT7   0x06
#define TR3_ANT8   0x07

#define TR3_ModeCommand   0x00
#define TR3_ModeAutoscan  0x01
#define TR3_ModeTriger    0x02
#define TR3_ModePolling   0x03
#define TR3_ModeEAS       0x24
#define TR3_ModeInventory 0x50
#define TR3_ModeRDLOOP    0x58

#define TR3_UID_SIZE  8
#define TR3_TAG_SIZE  16

//ユーザ設定値
#define TR3_TAG_MAX  40	//HardwareMAX: 200
#define TR3_MAX_COMMAND_SIZE  30
#define TR3_USED_ANT_NUM  2

#define IC_STAGE_NAME_SIZE  100
#define IC_TAG_MAX  20
#define IC_STAGES_MAX  2
#define IC_OBJECT_IN     1
#define IC_OBJECT_OUT   -1
#define IC_OBJECT_MOVE   2
#define IC_OBJECT_STAY   0

#define NONE  0
#define EXIST 1

//------------------------------------------------------------------------------
class CLoadCell
{
	private:
		struct termios oldtio, newtio;  /* 通信ポートを制御するためのインターフェイス */
		bool Close();
		int mPreSensorsWeight[LC_MAX_SENSOR_NUM];
		float mSensorPosX[LC_MAX_SENSOR_NUM];
		float mSensorPosY[LC_MAX_SENSOR_NUM];

	public:
		CLoadCell(){};
        ~CLoadCell(){Close();};
		bool Setup();
		int mSensorNum;
		void ResetWeight(int initial[] = NULL, int num = 10);
		void SetSensorPos(int sensor_num, float x[], float y[]);
		int GetWeight(int sensor_id);
		int GetWeightDiff(float *x, float *y, int diffs[], int threshold = 20);
		int fd;
};

//------------------------------------------------------------------------------
class CTR3
{
	private:
		struct termios oldtio, newtio;  /* 通信ポートを制御するためのインターフェイス */
		bool Close();
		unsigned char mCommand[TR3_MAX_COMMAND_SIZE];
		int  AddChecksum();
		int  mActiveAntenna;
		int  Inventory2();
	public:
		CTR3(){};
        ~CTR3(){Close();};
		int  mTagNum[TR3_USED_ANT_NUM];
		unsigned char mTagUIDs[TR3_USED_ANT_NUM][TR3_TAG_MAX][TR3_UID_SIZE];
		std::vector<std::string> mUIDs[TR3_USED_ANT_NUM];

		bool Setup();
		bool SetMode(unsigned char mode = TR3_ModeCommand);
		int  SetAntenna(unsigned char AN = TR3_ANT1);
		bool AntennaPowerON();
		bool AntennaPowerOFF();
		void PrintTagUIDs();

		int  GetTagDiff(std::string &diffUID, unsigned char AN);
		int  GetTagDiff2(std::vector<std::string> &inctag, std::vector<std::string> &dectag, unsigned char AN);
		int fd;
};

//------------------------------------------------------------------------------
class CTagOBJ
{
	private:
		bool Setup();
		bool Close();

	public:
		CTagOBJ(){Setup();};
        ~CTagOBJ(){Close();};

		std::string mUID;
		int mWeight;
		int mDiffs[LC_MAX_SENSOR_NUM];
		float mX;
		float mY;
		std::string  mName;
		std::string  mComment;
		int fd;
};

//------------------------------------------------------------------------------
class CStage
{
	private:
		bool Close();

	public:
		CStage(){};
		CLoadCell cLoadCell;
		unsigned char mAntenna;
		int mStagePos[3];
		char mName[IC_STAGE_NAME_SIZE];
		int  mObjNum;
		std::vector<CTagOBJ> cTagObj;

		bool Setup();
		void SetSensorPos(int sensor_num, float x[], float y[]);
		void SetAntenna(unsigned char AN);
		int fd;
};

//------------------------------------------------------------------------------
class CIntelCab
{
	private:
		bool Setup(int stage_num);
		bool Close();

	public:
		CIntelCab(int stage_num = IC_STAGES_MAX){Setup(stage_num);};
        ~CIntelCab(){Close();};
		CTR3 cTR3;
		CStage cStage[IC_STAGES_MAX];
		int mStageNum;

		void PrintObjInfo();
		int UpdateObj(int stageNo, CTagOBJ *cInOut);
		int UpdateObj2(int No, std::vector<std::string> &inctag, std::vector<std::string> &dectag);
		int fd;
};

//------------------------------------------------------------------------------
bool CLoadCell::Setup()
{
	//数値初期化関連
	for(int i=0; i<LC_MAX_SENSOR_NUM; i++){
		mPreSensorsWeight[i]= 0;
		mSensorPosX[i] = 0;
		mSensorPosY[i] = 0;
	}
	mSensorNum = LC_MAX_SENSOR_NUM;

	//通信関連初期化
	if( (fd = open(PORT_LC0.c_str(), (O_RDWR | O_NOCTTY) )) < 0 ){
		printf("FAILED: LoadCell->OpenComPort:PORT_LC0 \n");
		exit(-1);
	}
	tcgetattr(fd, &oldtio);         /* 現在のシリアルポートの設定を待避させる*/
	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 15;            /* 15文字受け取るまでブロックする */
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	std::cout << "OPENED: LoadCell(port:" << PORT_LC0.c_str() << ")" << std::endl;

	tcsetattr(fd, TCSANOW, &oldtio);  /* 退避させた設定に戻す */
	close(fd);

	ResetWeight();
	
	return true;
}

//------------------------------------------------------------------------------
bool CLoadCell::Close()
{
	close(fd);

	std::cout << "CLOSED: LoadCell" << std::endl;
	return true;
}

//------------------------------------------------------------------------------
void CLoadCell::SetSensorPos(int sensor_num, float x[], float y[])
{
	mSensorNum = sensor_num;
	for(int i=0; i<mSensorNum; i++){
		mSensorPosX[i] = x[i];
		mSensorPosY[i] = y[i];
	}
}

//------------------------------------------------------------------------------
// 指定されたセンサから重さを取得する
int CLoadCell::GetWeight(int sensor_id)
{
	char signal, buf[20];

	memset(&buf, 0, sizeof(buf));
	
	// センサ番号からセンサのアドレスを求める
	if(0<=sensor_id && sensor_id<=9)
		signal=sensor_id+'0';	// 0->'0', 1->'1', ..., 9->'9'
	else
		signal=sensor_id-10+'A';	// 10->'A', 11->'B', ... , 35->'Z'

	//通信関連初期化
	if( (fd = open(PORT_LC0.c_str(), (O_RDWR | O_NOCTTY) )) < 0 ){
		printf("FAILED: LoadCell->OpenComPort:PORT_LC0 \n");
		exit(-1);
	}

	tcgetattr(fd, &oldtio);         /* 現在のシリアルポートの設定を待避させる*/
	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 15;            /* 15文字受け取るまでブロックする */
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);
	write(fd, &signal, 1);
	read(fd, buf, 15);

	tcsetattr(fd, TCSANOW, &oldtio);  /* 退避させた設定に戻す */
	close(fd);

	int output_value = atoi(buf);

	// センサ出力を校正して返す
	return (int)(output_value*5);	//0.28は経験的な値
}

//------------------------------------------------------------------------------
void CLoadCell::ResetWeight(int initial[], int num)
{
	int i,j;

	if(initial != NULL){
		for(i=0; i<mSensorNum; i++)
			mPreSensorsWeight[i] = initial[i];
			return;
	}

	for(i=0; i<mSensorNum; i++)
		mPreSensorsWeight[i] = 0;

	for(i=0; i<num; i++)
		for(j=0; j<mSensorNum; j++)
			mPreSensorsWeight[j] += GetWeight(j);

	for(i=0; i<mSensorNum; i++)
		mPreSensorsWeight[i] /= num;
}

//------------------------------------------------------------------------------
//重量の増減（物体の増減）があるかをチェック
int  CLoadCell::GetWeightDiff(float *x, float *y, int diffs[], int threshold)
{
	static int i,j,cnt,weight;
	static int now, pre[LC_MAX_SENSOR_NUM], buf[LC_GET_WEIGHT_CNT][LC_MAX_SENSOR_NUM], latest[LC_MAX_SENSOR_NUM];
	

	//出力が安定するまで待つ
	for(i=0; i<mSensorNum; i++) {
		pre[i] = GetWeight(i);
	}
	cnt = 0;   	//繰り返し回数
	/*------------------------------------------*/
	do{
		//usleep(4000);	//4ms程度は間隔を空ける
		usleep(20000);	//for demo
		weight = 0;
		for(i=0; i<mSensorNum; i++) {
			now = GetWeight(i);
			weight += abs(now - pre[i]);
			pre[i] = now;
			buf[cnt][i] = now;
		}
		if(weight < LC_GET_WEIGHT_STABLE) {
			cnt++;
		} else {
			cnt=0;
		}
	}while(cnt < LC_GET_WEIGHT_CNT);
	/*------------------------------------------*/

	//出力
	for(i=0; i<mSensorNum; i++) {
		pre[i] = 0;
	}
	for(i=0; i<LC_GET_WEIGHT_CNT; i++){
		for(j=0; j<mSensorNum; j++) {
			pre[j] += buf[i][j];
		}
	}
	for(i=0; i<mSensorNum; i++) {
		pre[i] /= LC_GET_WEIGHT_CNT;
		diffs[i] = pre[i] - mPreSensorsWeight[i];
	}

	*x = *y = 0;
	weight = 0;
	for(i=0; i<mSensorNum; i++){
		*x += (float)(mSensorPosX[i] * abs(diffs[i]));
		*y += (float)(mSensorPosY[i] * abs(diffs[i]));
		weight += diffs[i];
	}
	*x /= abs(weight);
	*y /= abs(weight);

	if(abs(weight) < threshold){
		return 0;
	}else{
		for(i=0; i<mSensorNum; i++) {
			mPreSensorsWeight[i] = pre[i];
		}
		return weight;
	}
}

//------------------------------------------------------------------------------
bool CTR3::Setup()
{
	//数値初期化関連
	memset(mTagUIDs, 0, sizeof(unsigned char)*TR3_USED_ANT_NUM*TR3_TAG_MAX*TR3_UID_SIZE);
	mActiveAntenna = TR3_ANT1;
	for(int i=0; i<TR3_USED_ANT_NUM; i++)
		mTagNum[i] = 0;
	
	mCommand[0] = TR3_STX;	//
	mCommand[1] = 0x00;		//アドレス
	
	if( (fd = open(PORT_TR.c_str(), (O_RDWR | O_NOCTTY) )) < 0 ){
		printf("ERROR!!\n");
		exit(-1);
	}

	tcgetattr(fd, &oldtio);         /* 現在のシリアルポートの設定を待避させる*/
	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	tcsetattr(fd, TCSANOW, &oldtio);  /* 退避させた設定に戻す */
	close(fd);

	std::cout << "OPENED: TR3(port:" << PORT_TR.c_str() << ")" << std::endl;
	
	return true;
}

//------------------------------------------------------------------------------
//終了処理
bool CTR3::Close()
{
	close(fd);
	
	std::cout << "CLOSED: TR3" << std::endl;
	return true;
}

//------------------------------------------------------------------------------
//アンテナの指定
int CTR3::SetAntenna(unsigned char AN)
{
	unsigned long num;
	unsigned char buf[100];

	mCommand[2] = 0x4E; //コマンド
	mCommand[3] = 0x02; //データ長
	mCommand[4] = 0x9C; //コマンド詳細
	mCommand[5] = AN; //アンテナ指定
	
	memset(&buf, 0, sizeof(buf));
	//通信関連初期化
	if( (fd = open(PORT_TR.c_str(), (O_RDWR | O_NOCTTY) )) < 0 ){
		printf("FAILED:TR3->OpenComPort: PORT_TR \n");
	}

	tcgetattr(fd, &oldtio);         /* 現在のシリアルポートの設定を待避させる*/
	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	AddChecksum();
	write(fd, mCommand, sizeof(mCommand));
	read(fd, buf, 9);
	if(buf[2] != TR3_ACK) {
		read(fd, buf, 100);
		std::cerr << "TR3: SendCommandError -> SetAntenna" << std::endl; 

		return -1;
	}
	mActiveAntenna = (int)buf[5];

	tcsetattr(fd, TCSANOW, &oldtio);  /* 退避させた設定に戻す */
	close(fd);

	return (int)buf[5];
}

//------------------------------------------------------------------------------
//アンテナの電源ON
bool CTR3::AntennaPowerON()
{
	////DWORD num;
	unsigned long num;
	unsigned char buf[100];
	unsigned char cmd[9] = {0x02, 0x00, 0x4e, 0x02, 0x9e, 0x01, 0x03, 0xf4, 0x0d};

	mCommand[2] = 0x4E; //コマンド
	mCommand[3] = 0x02; //データ長
	mCommand[4] = 0x9E; //コマンド詳細
	mCommand[5] = 0x01; //パワーON

	memset(&buf, 0, sizeof(buf));

	//通信関連初期化
	if( (fd = open(PORT_TR.c_str(), (O_RDWR | O_NOCTTY) )) < 0 ){
		printf("FAILED:TR3->OpenComPort: PORT_TR \n");
		exit(-1);
	}

	tcgetattr(fd, &oldtio);         /* 現在のシリアルポートの設定を待避させる*/
	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	AddChecksum();
	write(fd, mCommand, sizeof(mCommand));
	read(fd, buf, 9);

	if(buf[2] != TR3_ACK) {
		read(fd, buf, 100);
		std::cerr << "TR3: SendCommandError -> AntennaPowerON" << std::endl; 
		return false;
	}

	tcsetattr(fd, TCSANOW, &oldtio);  /* 退避させた設定に戻す */
	close(fd);

	return true;
}

//------------------------------------------------------------------------------
//アンテナの電源OFF
bool CTR3::AntennaPowerOFF()
{
	unsigned long num;
	unsigned char buf[100];

	mCommand[2] = 0x4E; //コマンド
	mCommand[3] = 0x02; //データ長
	mCommand[4] = 0x9E; //コマンド詳細
	mCommand[5] = 0x00; //パワーOFF

	memset(&buf, 0, sizeof(buf));

	//通信関連初期化
	if( (fd = open(PORT_TR.c_str(), (O_RDWR | O_NOCTTY) )) < 0 ){
		printf("FAILED:TR3->OpenComPort: PORT_TR \n");
		exit(-1);
	}

	tcgetattr(fd, &oldtio);         /* 現在のシリアルポートの設定を待避させる*/
	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	AddChecksum();
	write(fd, mCommand, sizeof(mCommand));
	read(fd, buf, 9);

	if( buf[2] != TR3_ACK ) {
		read(fd, buf, 100);
		std::cerr << "TR3: SendCommandError -> AntennaPowerOFF" << std::endl; 
		return false;
	}

	tcsetattr(fd, TCSANOW, &oldtio);  /* 退避させた設定に戻す */
	close(fd);

	return true;
}

//------------------------------------------------------------------------------
//各アンテナで計測されているタグIDを全て表示
void CTR3::PrintTagUIDs()
{
	int i,j;

	for(i=0; i<TR3_USED_ANT_NUM; i++) {
		std::cout << "\n:::: ANTENNA " << i+1 << " ::::"<< std::endl;
		for(j=0; j<mTagNum[i]; j++) {
			std::cout << std::setw(3) << j+1 << "-> " << mUIDs[i][j] << std::endl;
		}
	}
}

//------------------------------------------------------------------------------
//タグの読み取り
int CTR3::Inventory2()
{	
//アンテナを変更しないと既読込のUIDは返さず，新規UIDのみ返す
	unsigned long num;
	int i,j;
	static unsigned char buf[TR3_TAG_SIZE*TR3_TAG_MAX];

	mCommand[2] = 0x78; //コマンド
	mCommand[3] = 0x03; //データ長
	mCommand[4] = 0xF0; //コマンド詳細
	mCommand[5] = 0x00; //アンチコリジョン有
	mCommand[6] = 0x01; //出力指定->取得データ数＋UIDデータ

	//通信関連初期化
	if( (fd = open(PORT_TR.c_str(), (O_RDWR | O_NOCTTY) )) < 0 ){
		printf("FAILED:TR3->OpenComPort: PORT_TR \n");
		exit(-1);
	}

	tcgetattr(fd, &oldtio);         /* 現在のシリアルポートの設定を待避させる*/
	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	AddChecksum();
	write(fd, mCommand, sizeof(mCommand));
	read(fd, buf, 9);

	if(buf[2] != TR3_ACK) {
		std::cerr << "TR3: SendCommandError -> Inventory2" << std::endl;
		usleep(100000);
		read(fd, buf, TR3_TAG_SIZE*TR3_TAG_MAX);
		return -1;
	}

	int tag_num = (int)buf[5];	//読み込むタグの数
//-------------
//タグ情報の読込
	for(i=0; i<tag_num; i++) {
		read(fd, buf, TR3_TAG_SIZE);		////****
		for(j=0; j<TR3_UID_SIZE; j++){
			mTagUIDs[mActiveAntenna][i][TR3_UID_SIZE-1-j] = buf[j+5];
		}
	}
	mTagNum[mActiveAntenna] = tag_num;

	tcsetattr(fd, TCSANOW, &oldtio);  /* 退避させた設定に戻す */
	close(fd);

	return tag_num;
}

//------------------------------------------------------------------------------
//タグの入出のチェック
int CTR3::GetTagDiff(std::string &diffUID, unsigned char AN)
{
	SetAntenna(AN);
	if(Inventory2() == -1){
		return 0;
	}

	std::vector<std::string> preUIDs = mUIDs[mActiveAntenna];
	mUIDs[mActiveAntenna].clear();

	for(int i=0; i<mTagNum[mActiveAntenna]; i++){
		char hex[17];
		sprintf(hex, "%02X%02X%02X%02X%02X%02X%02X%02X", 
			mTagUIDs[mActiveAntenna][i][0], mTagUIDs[mActiveAntenna][i][1], mTagUIDs[mActiveAntenna][i][2], mTagUIDs[mActiveAntenna][i][3],
			mTagUIDs[mActiveAntenna][i][4], mTagUIDs[mActiveAntenna][i][5], mTagUIDs[mActiveAntenna][i][6], mTagUIDs[mActiveAntenna][i][7]);
		mUIDs[mActiveAntenna].push_back(std::string(hex));
	}
	mTagNum[mActiveAntenna] = (int)mUIDs[mActiveAntenna].size();

	//タグの増減の確認
	std::sort(mUIDs[mActiveAntenna].begin(), mUIDs[mActiveAntenna].end());
	// IDにあってpreIDにない => 追加された物品ID
	std::vector<std::string> increase;
	std::set_difference(mUIDs[mActiveAntenna].begin(), mUIDs[mActiveAntenna].end(), preUIDs.begin(), preUIDs.end(), 
		std::inserter(increase, increase.begin()));

	// preIDにあってIDにない => 取り除かれた物品ID
	std::vector<std::string> decrease;
	std::set_difference(preUIDs.begin(), preUIDs.end(), mUIDs[mActiveAntenna].begin(), mUIDs[mActiveAntenna].end(), 
		std::inserter(decrease, decrease.begin()));

	// 増減なし
	if( (increase.size() == 0) && (decrease.size() == 0) ){
		return 0;
	}
	// 物品追加
	if( (increase.size() == 1) && (decrease.size() == 0) ){
		diffUID = increase[0];
		return 1;
	}
	// 物品除去
	if( (increase.size() == 0) && (decrease.size() == 1) ){
		diffUID = decrease[0];
		return -1;
	}
	// 複数物品の同時入出時（１個ずつ検出するようにする）
	if(increase.size() >= 1){
		preUIDs.push_back(increase[0]);
		mUIDs[mActiveAntenna] = preUIDs;
		std::sort(mUIDs[mActiveAntenna].begin(), mUIDs[mActiveAntenna].end());
		mTagNum[mActiveAntenna] = (int)mUIDs[mActiveAntenna].size();
		diffUID = increase[0];
		return 1;
	}
	if(decrease.size() >= 1){
		preUIDs.erase(remove(preUIDs.begin(), preUIDs.end(), decrease[0]),  preUIDs.end());
		mUIDs[mActiveAntenna] = preUIDs;
		std::sort(mUIDs[mActiveAntenna].begin(), mUIDs[mActiveAntenna].end());
		mTagNum[mActiveAntenna] = (int)mUIDs[mActiveAntenna].size();
		diffUID = decrease[0];
		return -1;
	}

	return 0;
}

//------------------------------------------------------------------------------
//タグの入出のチェック
int CTR3::GetTagDiff2(std::vector<std::string> &inctag, std::vector<std::string> &dectag, unsigned char AN)
{
	SetAntenna(AN);
	if(Inventory2() == -1){
		return 0;
	}

	std::vector<std::string> preUIDs = mUIDs[mActiveAntenna];
	mUIDs[mActiveAntenna].clear();

	for(int i=0; i<mTagNum[mActiveAntenna]; i++){
		char hex[17];
		sprintf(hex, "%02X%02X%02X%02X%02X%02X%02X%02X", 
			mTagUIDs[mActiveAntenna][i][0], mTagUIDs[mActiveAntenna][i][1], mTagUIDs[mActiveAntenna][i][2], mTagUIDs[mActiveAntenna][i][3],
			mTagUIDs[mActiveAntenna][i][4], mTagUIDs[mActiveAntenna][i][5], mTagUIDs[mActiveAntenna][i][6], mTagUIDs[mActiveAntenna][i][7]);
		mUIDs[mActiveAntenna].push_back(std::string(hex));
	}
	mTagNum[mActiveAntenna] = (int)mUIDs[mActiveAntenna].size();

	//タグの増減の確認
	std::sort(mUIDs[mActiveAntenna].begin(), mUIDs[mActiveAntenna].end());
	// IDにあってpreIDにない => 追加された物品ID
	std::vector<std::string> increase;
	std::set_difference(mUIDs[mActiveAntenna].begin(), mUIDs[mActiveAntenna].end(), preUIDs.begin(), preUIDs.end(), 
		std::inserter(increase, increase.begin()));

	// preIDにあってIDにない => 取り除かれた物品ID
	std::vector<std::string> decrease;
	std::set_difference(preUIDs.begin(), preUIDs.end(), mUIDs[mActiveAntenna].begin(), mUIDs[mActiveAntenna].end(), 
		std::inserter(decrease, decrease.begin()));

	// 物品の入出庫
	std::sort(mUIDs[mActiveAntenna].begin(), mUIDs[mActiveAntenna].end());
	mTagNum[mActiveAntenna] = (int)mUIDs[mActiveAntenna].size();
	if(increase.size() >= 1){
		std::sort(increase.begin(), increase.end());
		inctag = increase;
	}
	if(decrease.size() >= 1){
		std::sort(decrease.begin(), decrease.end());
		dectag = decrease;
	}

	return mTagNum[mActiveAntenna];
}

//------------------------------------------------------------------------------
//通信用サブ関数
int CTR3::AddChecksum()
{
	int num = (int)mCommand[3] +5;

	mCommand[num-1] = TR3_ETX;
	mCommand[num+1] = TR3_CR;

	mCommand[num] = 0x00;
	for(int i= 0; i<num; i++) {
		mCommand[num] += mCommand[i];
	}

	return num+2;
}

//------------------------------------------------------------------------------
//38.4kbps対応のためのEEPROMの書き換えに使用
bool CTR3::SetMode(unsigned char mode)
{
	unsigned long num;
	unsigned char buf[100];


	mCommand[2] = 0x4F; //コマンド
	mCommand[3] = 0x01; //データ長
	mCommand[4] = 0x00; //コマンド詳細
	//通信関連初期化
	if( (fd = open(PORT_TR.c_str(), (O_RDWR | O_NOCTTY) )) < 0 ){
		printf("FAILED:TR3->OpenComPort: PORT_TR \n");
		exit(-1);
	}
	tcgetattr(fd, &oldtio);         /* 現在のシリアルポートの設定を待避させる*/
	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	AddChecksum();
	write(fd, mCommand, sizeof(mCommand));
	printf("NOW Mode: ");
	read(fd, buf, 100);
	for(int i= 0; i<(int)num; i++)
		printf("%02x ",buf[i]);
	printf("\n");

	mCommand[2] = 0x4E; //コマンド
	mCommand[3] = 0x07; //データ長
	mCommand[4] = 0x10; //コマンド詳細   EEPROMへ書込
	mCommand[5] = mode; //モード指定 
	mCommand[6] = 0x00; // 0固定
	mCommand[7] = 0x98; // default->0x18 から 38.4Kbps対応へ変更
	mCommand[8] = 0x00; // 予約
	mCommand[9] = 0x00; // 予約 
	mCommand[10]= 0x00; // 予約
	
	AddChecksum();
	write(fd, mCommand, sizeof(mCommand));
	read(fd, buf, 100);
	for(int i= 0; i<(int)num; i++)
		printf("%02x ",buf[i]);
	printf("\n");

	mCommand[2] = 0x4F; //コマンド
	mCommand[3] = 0x01; //データ長
	mCommand[4] = 0x00; //コマンド詳細
	AddChecksum();
	write(fd, mCommand, sizeof(mCommand));
	printf("Revised Mode: ");
	read(fd, buf, 100);
	for(int i= 0; i<(int)num; i++)
		printf("%02x ",buf[i]);
	printf("\n");

	tcsetattr(fd, TCSANOW, &oldtio);  /* 退避させた設定に戻す */
	close(fd);

	exit(0);

	return false;
}

//------------------------------------------------------------------------------
bool CTagOBJ::Setup()
{
	char id[TR3_UID_SIZE*2+1] = {'\0'};
	for(int i=0; i<TR3_UID_SIZE*2; i++)
		id[i] = 0x00;
	mUID.assign(id);

	for(int i=0; i<LC_MAX_SENSOR_NUM; i++)
		mDiffs[i] = 0;

	mX = mY = 0;
	mWeight = 0;
	
	mName = '\0';
	mComment = '\0';

	return true;
}

//------------------------------------------------------------------------------
bool CTagOBJ::Close()
{
	return true;
}

//------------------------------------------------------------------------------
bool CStage::Setup()
{
	cLoadCell.Setup();

	mObjNum = 0;
	for(int i=0; i<3; i++){
		mStagePos[i] = 0;
	}

	return true;
}

//------------------------------------------------------------------------------
bool CStage::Close()
{
	return true;
}

//------------------------------------------------------------------------------
void CStage::SetSensorPos(int sensor_num, float x[], float y[])
{
	cLoadCell.SetSensorPos(sensor_num, x, y);
}

//------------------------------------------------------------------------------
void CStage::SetAntenna(unsigned char AN)
{
	mAntenna =  AN;
}

//------------------------------------------------------------------------------
bool CIntelCab::Setup(int stage_num)
{
	mStageNum = stage_num;
	return true;
}

//------------------------------------------------------------------------------
bool CIntelCab::Close()
{
	return true;
}

//------------------------------------------------------------------------------
void CIntelCab::PrintObjInfo()
{
	CTagOBJ  *cObj;
	
	for(int i=0; i<mStageNum; i++){
		std::cout << "\n" << std::setw(20) << std::setfill(':') << cStage[i].mName << "::::::::::" << std::endl;
		for(int j=0; j<cStage[i].mObjNum; j++) {
			cObj = &(cStage[i].cTagObj[j]);
			printf("%3d:  UID->", j+1);
			std::cout << cObj->mUID;
			printf("  Weight=%4d  X=%4.0f Y=%4.0f", cObj->mWeight, cObj->mX, cObj->mY);
			std::cout << " <" << cObj->mName << ":" << cObj->mComment << ">" << std::endl; 
		}
	}
}

//------------------------------------------------------------------------------
int CIntelCab::UpdateObj(int No, CTagOBJ  *cInOut)
{
	static CTagOBJ  cObj, cObjIn[IC_STAGES_MAX], cObjOut[IC_STAGES_MAX];
	static int InOutTag[IC_STAGES_MAX], InOutLC[IC_STAGES_MAX];
	int value = IC_OBJECT_STAY;

	if(No >= mStageNum)
		return IC_OBJECT_STAY;

	//タグの増減チェック
	cTR3.AntennaPowerON();
	if(mStageNum == 1){
		cTR3.SetAntenna(cStage[No].mAntenna +1);
	}
	int inout = cTR3.GetTagDiff(cObj.mUID, cStage[No].mAntenna);
	cTR3.AntennaPowerOFF();

	//タグ検出
	if(inout > 0) {
		InOutTag[No] = 1;
		cObjIn[No] = cObj;
	}
	//出庫
	else if( inout < 0 ){
		for(int i=0; i<cStage[No].mObjNum; i++){
			if(cStage[No].cTagObj.at(i).mUID.compare(cObj.mUID) == 0){
				cStage[No].cTagObj.erase(cStage[No].cTagObj.begin()+i);
				cStage[No].mObjNum = (int)cStage[No].cTagObj.size();
				InOutLC[No] = 0;
				break;
			}
		}
		InOutTag[No] = 0;
		*cInOut = cObj;
		value = IC_OBJECT_OUT;
	}

//ロードセルの増減チェック
	cObj.mWeight = cStage[No].cLoadCell.GetWeightDiff(&cObj.mX, &cObj.mY, cObj.mDiffs);

	if( (cObj.mWeight > 0) && (InOutTag[No] > 0) ){
	//入庫
		cObj.mUID = cObjIn[No].mUID;
		cStage[No].cTagObj.push_back(cObj);
		cStage[No].mObjNum = (int)cStage[No].cTagObj.size();
		InOutTag[No] = 0;
		InOutLC[No]  = 0;
		*cInOut = cObj;
		value = IC_OBJECT_IN;
	}
	else if( (cObj.mWeight > 0) && (InOutLC[No] < 0) ) {
	//庫内移動
		int cnt = TR3_TAG_MAX;
		for(int i=0; i<cStage[No].mObjNum; i++) {
			if(cStage[No].cTagObj.at(i).mUID.compare(cObjOut[No].mUID) == 0){
				cnt = i;
				break;
			}
		}
		if(cnt != TR3_TAG_MAX){
			cStage[No].cTagObj.at(cnt) = cObj;
			cStage[No].cTagObj.at(cnt).mUID     = cObjOut[No].mUID;
			cStage[No].cTagObj.at(cnt).mName    = cObjOut[No].mName;
			cStage[No].cTagObj.at(cnt).mComment = cObjOut[No].mComment;
			InOutLC[No] = 0;
			*cInOut = cStage[No].cTagObj.at(cnt);
			value = IC_OBJECT_MOVE;
		}
	}
	//タグ無し物品の入庫
	else{
	}

	//持ち上げ
	if(cObj.mWeight < 0){
		int comp = 5000;
		int cnt = TR3_TAG_MAX;
		for(int i=0; i<cStage[No].mObjNum; i++){
			int sum =0;
			for(int j=0; j<LC_MAX_SENSOR_NUM; j++) {
				sum += abs(abs(cStage[No].cTagObj.at(i).mDiffs[j]) - abs(cObj.mDiffs[j]));
			}
			if(sum < comp) {
				comp = sum;
				cnt = i;
			}
		}
		if(cnt != TR3_TAG_MAX){
			cObjOut[No] = cStage[No].cTagObj.at(cnt);
			InOutLC[No] = -1;
		}
	}
	return value;
}

//------------------------------------------------------------------------------
int CIntelCab::UpdateObj2(int No, std::vector<std::string> &inctag, std::vector<std::string> &dectag)
{
	std::vector<std::string> increase, decrease;

	if(No >= mStageNum)
		return 0;

//タグの増減チェック
	cTR3.AntennaPowerON();
	if(mStageNum == 1){
		cTR3.SetAntenna(cStage[No].mAntenna +1);
	}
	cTR3.GetTagDiff2(increase, decrease, cStage[No].mAntenna);
	cTR3.AntennaPowerOFF();

	//出庫
	for(unsigned int i=0; i<decrease.size(); i++){
		for(unsigned int j=0; j<cStage[No].cTagObj.size(); j++) {
			if(cStage[No].cTagObj.at(j).mUID.compare( decrease.at(i) ) == 0){
				cStage[No].cTagObj.erase(cStage[No].cTagObj.begin()+j);
				break;
			}
		}
	}
	//入庫
	for(unsigned int i=0; i<increase.size(); i++) {
		CTagOBJ  cObj;
		cObj.mUID = increase.at(i);
		cStage[No].cTagObj.push_back(cObj);
	}
	//出入庫有
	if( (decrease.size() != 0) || (increase.size() != 0) ){
		cStage[No].mObjNum = (int)cStage[No].cTagObj.size();
	}
	inctag = increase;
	dectag = decrease;

	return cStage[No].mObjNum;
}

//------------------------------------------------------------------------------
int main(int argc, char** argv)
{
	std::map<std::string, int32_t> rfidValue;
	rfidValue["E00401004E17F97A"]=7001;
	rfidValue["E00401004E180E50"]=7002;
	rfidValue["E00401004E180E58"]=7003;
	rfidValue["E00401004E180E60"]=7004;
	rfidValue["E00401004E180E68"]=7005;
	rfidValue["E00401004E180EA0"]=7006;
	rfidValue["E00401004E180EA8"]=7007;
	rfidValue["E00401004E181C88"]=7008;
	rfidValue["E00401004E181C87"]=7009;
	rfidValue["E00401004E181C7F"]=7010;
	rfidValue["E00401004E181C77"]=7011;
	rfidValue["E00401004E181C3F"]=7012;
	rfidValue["E00401004E181C37"]=7013;
	rfidValue["E00401004E180E47"]=7014;
	rfidValue["E00401004E180E3F"]=7015;
	rfidValue["E00401004E180E37"]=7016;
	rfidValue["E00401004E1805BD"]=7017;
	rfidValue["E00401004E180585"]=7018;
	rfidValue["E00401004E18057D"]=7019;
	rfidValue["E00401004E17EF3F"]=7020;
	rfidValue["E00401004E17EF37"]=7021;
	rfidValue["E00401004E17EF2F"]=7022;
	rfidValue["E00401004E17EF27"]=7023;
	rfidValue["E00401004E17EEEF"]=7024;
	rfidValue["E00401004E17EEE7"]=7025;

	CIntelCab cIntelCab(1);
	float xpos0[] = {16, 407, 16, 407}, ypos0[] = {16, 16, 244, 244}; // colorbox
	float xpos1[] = {16, 407, 16, 407}, ypos1[] = {16, 16, 244, 244}; // table
	CTagOBJ  cObj;
	char uid[100];

	char mes[30];
	int imageno = 0;

  tms_msg_db::TmsdbStamped 	icsmsg;
  tms_msg_db::Tmsdb 				tmpdata;
  // int32_t idSensor = 3002; // ics
  // int32_t idPlace  = 6010; // shelf (ics) >> 928_foor >> 928_room
    int32_t idSensor,idPlace; // shelf (ics) >> 928_foor >> 928_room

	ros::init(argc, argv, "ics",ros::init_options::AnonymousName);

	ros::NodeHandle n;
	ros::Publisher ics_pub 			= n.advertise<tms_msg_db::TmsdbStamped>("tms_db_data", 10);

    ros::NodeHandle nh_param("~");
    nh_param.param<std::string>("PORT_TR",PORT_TR,"/dev/ttyUSB0");
    nh_param.param<std::string>("PORT_LC0",PORT_LC0,"/dev/ttyACM0");
    // nh_param.param<int32_t>("idSensor",idSensor,NULL);
    // nh_param.param<int32_t>("idPlace",idPlace,NULL);
	if(! nh_param.getParam("idSensor",idSensor)){
		ROS_ERROR("ros param idSensor isn't exist");
		return 0;
	}
	if(! nh_param.getParam("idPlace",idPlace)){
		ROS_ERROR("ros param idPlace isn't exist");
		return 0;
	}

	//iniファイルから値を読み込んで各デバイスに接続
	//RFIDリーダ接続
	char ComPort[32];
	cIntelCab.cTR3.Setup();
	cIntelCab.cTR3.AntennaPowerOFF();
	cIntelCab.cStage[0].SetAntenna(TR3_ANT1);
	cIntelCab.cStage[1].SetAntenna(TR3_ANT2);

	//ロードセル接続
	char StageName[32];
	cIntelCab.cStage[0].Setup();
	cIntelCab.cStage[0].SetSensorPos(4, xpos0, ypos0);
	cIntelCab.cStage[0].mStagePos[0] = 0;
	cIntelCab.cStage[0].mStagePos[1] = 0;
	cIntelCab.cStage[0].mStagePos[2] = 830;

	//初回時の起動は多少時間がかかるためここで一回実行しておく
	for(int i=0; i<cIntelCab.mStageNum; i++){
		cIntelCab.UpdateObj(i, &cObj);
	}

	//計測開始
	bool change_flag = false;
	int index = 0;
	std::cout << "\nSTART" << std::endl;

	while(1)
	{
		// vector 初期化 
		// 毎回初期化し，庫内にある物品だけ値を更新して送信する 
	  ros::Time now = ros::Time::now() + ros::Duration(9*60*60); // GMT +9
	  // icsmsg.header.frame_id  = "/ics";
	  // nh_param.param<std::string>("frame_id",icsmsg.header.frame_id,NULL);
	  if(! nh_param.getParam("frame_id",icsmsg.header.frame_id)){
	  	ROS_ERROR("ros param frame_id isn't exist");
	  	return 0;
	  }
	  icsmsg.header.stamp     = now;

    icsmsg.tmsdb.clear();
		for(int i = 0; i < MAX_OBJECT_NUM; i++){
      usleep(1000);	//1ms
      now = ros::Time::now() + ros::Duration(9*60*60); // GMT +9
		  tmpdata.time	= boost::posix_time::to_iso_extended_string(now.toBoost());
		  tmpdata.id		= i+7001;	//物品IDは 7001 から
		  tmpdata.x			= -1.0;
		  tmpdata.y			= -1.0;
		  tmpdata.z			= -1.0;
		  tmpdata.place	= idPlace;
		  tmpdata.sensor= idSensor;
		  tmpdata.state = NONE;	//知的収納庫内に 0:存在しない, 1:存在する
		  icsmsg.tmsdb.push_back(tmpdata);
		}

		for(int i=0; i<cIntelCab.mStageNum; i++){
			//増減の確認
			switch(cIntelCab.UpdateObj(i, &cObj)) {
				case  IC_OBJECT_STAY:
          change_flag = false; 
					break;
				case  IC_OBJECT_IN: 
					////Beep(2500,50);
					std::cout << "\n\n IN : ";
					index = (int)cIntelCab.cStage[i].cTagObj.size()-1;
					cIntelCab.cStage[i].cTagObj.at(index).mName    = cObj.mName;
					cIntelCab.cStage[i].cTagObj.at(index).mComment = cObj.mComment;
					change_flag = true;
					break;
				case  IC_OBJECT_MOVE: 
					////Beep(2500,50);
					std::cout << "\n\nMOVE: ";
					change_flag = true;
					break;
				case IC_OBJECT_OUT: 
					////Beep(2500,50); Sleep(50); Beep(2500,50);
					std::cout << "\n\n OUT: ";
					change_flag = true;
					break;
				default:
          change_flag = false;
					break;
			}

			if(change_flag) {
        
        change_flag = false;

			  int32_t vi = 255;
			  for(int j=0; j<cIntelCab.cStage[i].mObjNum; j++) {
				  cObj = cIntelCab.cStage[i].cTagObj[j];
				  vi = rfidValue[cObj.mUID] - 7001;
          usleep(1000);	//1ms
		      now = ros::Time::now() + ros::Duration(9*60*60); // GMT +9    
				  icsmsg.tmsdb[vi].time		= boost::posix_time::to_iso_extended_string(now.toBoost()); 
				  icsmsg.tmsdb[vi].id		  = rfidValue[cObj.mUID];		
			    icsmsg.tmsdb[vi].state  = EXIST;						//知的収納庫内に 0:存在しない, 1:存在する
			    icsmsg.tmsdb[vi].x			= cObj.mX;
			    icsmsg.tmsdb[vi].y			= cObj.mY;
			    icsmsg.tmsdb[vi].weight = cObj.mWeight;
			    // nh_param.param<float>("z",icsmsg.tmsdb[vi].z,NULL);
			    if(! nh_param.getParam("z",icsmsg.tmsdb[vi].z)){
			   		ROS_ERROR("ros param z isn't exist");
			   		return 0;
			    }
			   //  if(i == 0)	icsmsg.tmsdb[vi].z  = 620.0;		//棚上段の場合
				  // else 				icsmsg.tmsdb[vi].z	= 330.0;		//棚中段の場合
			  }
				cIntelCab.PrintObjInfo();
        ics_pub.publish(icsmsg);
			}
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// EOF