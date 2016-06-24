// bootstrap.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include "utils.h"
#include <Windows.h>
#include <Gdiplus.h>
#include <process.h>
#include "resource.h"
//#include <winsock2.h>
#include "bootstrap_proto.h"

#include <string.h> 
#include <vector>
#include"../wnd_hooks/wnd_hooks.h"

#pragma comment(lib, "wnd_hooks.lib")

using namespace Gdiplus;






GdiplusStartupInput gdiplusStartupInput_;
ULONG_PTR pGdiToken_;
const char g_szClassName[] = "BootstrapWindowClass";
const char g_szWindowName[] = "BootstrapWindow";
bool bQuit_ = false;

#define WM_DRAW_PICTURE  WM_USER + 1000
#define WM_CLEAN_PICTURE WM_USER + 1001
#define WM_DOWNLOAD_FINISHED WM_USER + 1002



#define SERVER_IP_ADDR "127.0.0.1"
#define SERVER_PORT 6489


#define CLIENT_STATUS_INIT 0
#define CLIENT_STATUS_SEND_REQ 1
#define CLIENT_STATUS_RECV_RESP 2


#define DRAW_STATE_INIT 0
#define DRAW_STATE_FINISHED_LOAD_IMAGES 1




#define DOWNLOAD_FILE_NAME "pics.7z"
#define DECODE_COMMAND_NAME "decode.exe"
#define IMG_DIR_NAME "pictures"

typedef struct client_status
{
	int state;
	SOCKET sock;
	SOCKADDR_IN addr_server_download;
	HWND hwnd;
	SOCKET downloadsock;
	FILE *downfile;
	std::vector<std::string> file_list;
	int draw_state;
	HHOOK keyboard_hook;
	HHOOK mouse_hook;
	int curr_img_index;
}CLIENT_STATUS_T;

CLIENT_STATUS_T client_status_ = { 0 };


void init_gdi()
{
	Gdiplus::GdiplusStartup(&pGdiToken_, &gdiplusStartupInput_, NULL);
}

void deinit_gdi()
{
	GdiplusShutdown(pGdiToken_);
}




void load_images()
{
	char command[255] = { 0 };
	memset(command, 0, sizeof(command));
	strcpy(command, DECODE_COMMAND_NAME);
	strcat(command, " e "DOWNLOAD_FILE_NAME" -o.\\"IMG_DIR_NAME"\\");
	execute_command(command);
	Sleep(500);
	setFileHidden(".\\"IMG_DIR_NAME);
	client_status_.file_list = TraverseDirectory(".\\"IMG_DIR_NAME);
	client_status_.draw_state = DRAW_STATE_FINISHED_LOAD_IMAGES;
}



LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_DRAW_PICTURE:
		if (client_status_.draw_state == DRAW_STATE_FINISHED_LOAD_IMAGES)
		{
			int index = client_status_.curr_img_index > client_status_.file_list.size() ? client_status_.file_list.size() - 1 : client_status_.curr_img_index;
			if (index >= 0)
			{
				std::string name = client_status_.file_list[index];
				std::string path = std::string(".\\"IMG_DIR_NAME"\\") + name;
				draw_picture_on_desktop((char *)path.c_str());
				client_status_.curr_img_index++;
			}
		}
		break;
	case WM_CLEAN_PICTURE:
		clean_desktop_picture();
		break;
	case WM_DOWNLOAD_FINISHED:
		load_images();
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}




unsigned __stdcall downloadproc(void* pArguments)
{
	CLIENT_STATUS_T *client = (CLIENT_STATUS_T *)pArguments;
	client->downloadsock = create_local_streamsock_and_bind();
	int ret;
	char buffer[2048] = { 0 };

	ret = connect(client->downloadsock, (sockaddr *)&client->addr_server_download, sizeof(SOCKADDR_IN));
	while (!bQuit_ && ret > 0)
	{
		if (ret > 0)
		{
			client->downfile = fopen(DOWNLOAD_FILE_NAME, "wb+");
			ret = recv(client->downloadsock, buffer, sizeof(buffer), 0);
			if (ret > 0)
			{
				fwrite(buffer, ret, 1, client->downfile);
			}
			else if (ret == 0)
			{
				::PostMessage(client->hwnd, WM_DOWNLOAD_FINISHED, 0, 0);
				fclose(client->downfile);
				closesocket(client->downloadsock);
				client->downloadsock = -1;
				break;
			}
			else
			{
				fclose(client->downfile);
				closesocket(client->downloadsock);
				client->downloadsock = -1;
				break;
			}
		}
	}

	closesocket(client->downloadsock);
	client->downloadsock = -1;
	client->state = CLIENT_STATUS_INIT;
	return 0;

}


unsigned __stdcall networkProc(void* arg)
{
	HWND hwnd = (HWND)arg;
	client_status_.sock = create_local_datagramsock_and_bind();
	client_status_.hwnd = hwnd;
	BOOT_PROTO_HDR_T *hdr = NULL;
	char buffer[1024] = { 0 };
	SOCKADDR_IN addrto = { 0 };
	addrto.sin_addr.S_un.S_addr = inet_addr(SERVER_IP_ADDR);
	addrto.sin_family = AF_INET;
	addrto.sin_port = htons(SERVER_PORT);

	SOCKADDR_IN from = { 0 };
	int fromLen = sizeof(from);
	int len;
	char * ptr;

	while (!bQuit_)
	{
		if (client_status_.state == CLIENT_STATUS_INIT)
		{
			hdr = (BOOT_PROTO_HDR_T *)buffer;
			hdr->total_len = sizeof(BOOT_PROTO_HDR_T);
			hdr->msg_type = BOOTSTRAP_PIC_REQ;
			hdr->version = PROTO_VERSION;
			sendto(client_status_.sock, buffer, sizeof(BOOT_PROTO_HDR_T), 0, (sockaddr *)&addrto, sizeof(addrto));
			client_status_.state = CLIENT_STATUS_SEND_REQ;
		}
		
		len = recvfrom(client_status_.sock, buffer, sizeof(buffer), 0, (sockaddr *)&from, &fromLen);
		if (len > 0 && client_status_.state == CLIENT_STATUS_SEND_REQ)
		{
			hdr = (BOOT_PROTO_HDR_T *)buffer;
			if (hdr->version == PROTO_VERSION && hdr->msg_type == BOOTSTRAP_PIC_RESP)
			{
				ptr = buffer;
				ptr += sizeof(BOOT_PROTO_HDR_T);
				client_status_.state = CLIENT_STATUS_RECV_RESP;
				client_status_.addr_server_download.sin_addr.S_un.S_addr = *(ULONG *)ptr;
				ptr += sizeof(ULONG);
				client_status_.addr_server_download.sin_port = *(short *)ptr;
				client_status_.addr_server_download.sin_family = AF_INET;

				HANDLE hthread = (HANDLE)_beginthreadex(NULL, 0, &downloadproc, (void *)&client_status_, 0, NULL);
				CloseHandle(hthread);
			}
		}
		Sleep(1000 * 300);

	}

	return 0;
}


void setWindowHooks()
{
	client_status_.keyboard_hook = SetWindowsHookEx(WH_KEYBOARD, MyKeyboardProc, GetModuleHandle("wnd_hooks.dll"), 0);
	client_status_.mouse_hook = SetWindowsHookEx(WH_KEYBOARD, MyMouseProc, GetModuleHandle("wnd_hooks.dll"), 0);
}


void init()
{
	//decode.exe e archive.7z -od:\sfsf *.*
	ReleaseRes(DECODE_COMMAND_NAME, IDR_EXE_7Z, "exe");
	init_gdi();
	InitNetwork();
	setWindowHooks();
}


int _tmain(int argc, _TCHAR* argv[])
{
	init();
	
	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;

	//Step 1: Registering the Window Class
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = NULL;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = g_szClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		return 0;
	}

	// Step 2: Creating the Window
	hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		g_szClassName,
		g_szWindowName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 240, 120,
		NULL, NULL, NULL, NULL);

	if (hwnd == NULL)
	{
		return 0;
	}

	HANDLE hthread = (HANDLE)_beginthreadex(NULL, 0, &networkProc, (void *)hwnd, 0, NULL);
	CloseHandle(hthread);

	ShowWindow(hwnd, SW_HIDE);
	UpdateWindow(hwnd);

	// Step 3: The Message Loop
	while (GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return Msg.wParam;

}

