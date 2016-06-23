// bootstrap.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"


#include <Windows.h>
#include <Gdiplus.h>
#include <process.h>
//#include <winsock2.h>


#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

GdiplusStartupInput gdiplusStartupInput_;
ULONG_PTR pGdiToken_;
const char g_szClassName[] = "BootstrapWindowClass";
const char g_szWindowName[] = "BootstrapWindow";
bool bQuit_ = false;

#define WM_DRAW_PICTURE  WM_USER + 1000
#define WM_CLEAN_PICTURE WM_DRAW_PICTURE + 1


typedef struct client_status
{
	int state;
	SOCKET sock;
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


void get_screen_width_height(int *width, int *height)
{
	 *width = GetSystemMetrics(SM_CXFULLSCREEN);
	 *height = GetSystemMetrics(SM_CYFULLSCREEN);
}


wchar_t * multibyte_to_wide_char(char * src)
{
	int len = strlen(src);
	wchar_t *buf = new wchar_t[len + 1];
	memset(buf, 0, (len + 1) *sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, src, strlen(src), buf, len);
	buf[len] = '\0';
	return buf;
}



void draw_picture_on_desktop(char *pic_path)
{
	int desk_width = 0;
	int desk_height = 0;
	get_screen_width_height(&desk_width, &desk_height);
	HWND wnd = ::GetDesktopWindow();
	int x, y;
	Image* pimage = NULL;
	wchar_t *picPath = multibyte_to_wide_char(pic_path);
	pimage = Image::FromFile(picPath);
	int w = pimage->GetWidth(); 
	int h = pimage->GetHeight(); 
	if (w < desk_width && h < desk_height)
	{
		x = desk_width / 2 - w / 2;
		y = desk_height / 2 - h / 2;
	}
	delete picPath;
	HDC hDeskDC = ::GetDC(0);//获取屏幕句柄
	Graphics gr(hDeskDC);
	gr.DrawImage(pimage, x, y);
	::ReleaseDC(wnd, hDeskDC);
}


void clean_desktop_picture()
{
	::RedrawWindow(NULL, NULL, NULL, 0);
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
		draw_picture_on_desktop("test.jpg");
		break;
	case WM_CLEAN_PICTURE:
		clean_desktop_picture();
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
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


unsigned __stdcall networkProc(void* arg)
{
	HWND hwnd = (HWND)arg;
	client_status_.sock = create_local_datagramsock_and_bind();
	while (!bQuit_)
	{

	}

	return 0;
}



int _tmain(int argc, _TCHAR* argv[])
{
	init_gdi();
	InitNetwork();

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

