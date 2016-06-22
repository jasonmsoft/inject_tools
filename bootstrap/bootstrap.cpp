// bootstrap.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <Gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

GdiplusStartupInput gdiplusStartupInput_;
ULONG_PTR pGdiToken_;
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
	memset(buf, 0, len + 1);
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
	Bitmap memBitmap(desk_width, desk_height);
	Graphics  memGr(&memBitmap);
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
	memGr.DrawImage(pimage, x, y, 0, 0, w, h, UnitPixel);
	delete pimage;
	pimage = NULL;
	delete picPath;

	HDC hDeskDC = ::GetDC(0);//获取屏幕句柄
	Graphics gr(hDeskDC);
	gr.DrawImage(&memBitmap, 0, 0);
	::ReleaseDC(wnd, hDeskDC);
}


int _tmain(int argc, _TCHAR* argv[])
{
	init_gdi();
	
	draw_picture_on_desktop("test.jpg");

	//deinit_gdi();
	system("pause");
	return 0;
}

