// wnd_hooks.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "wnd_hooks.h"
#include <process.h>
#include <queue>
#include "time.h" 
using namespace std;

queue<int> msgQueue_;
bool bQuit_ = false;
CRITICAL_SECTION cs_;//CRITICAL_SECTION struct
time_t last_receive_msg_time = 0;

#define MAX_IDLE_TIME  300

// 这是导出变量的一个示例
WND_HOOKS_API int nwnd_hooks=0;

// 这是导出函数的一个示例。
WND_HOOKS_API int fnwnd_hooks(void)
{
	return 42;
}


// 这是已导出类的构造函数。
// 有关类定义的信息，请参阅 wnd_hooks.h
Cwnd_hooks::Cwnd_hooks()
{
	return;
}

HWND targetwindow_ = NULL;
#define WM_DRAW_PICTURE  WM_USER + 1000
#define MSG_USER_IDLE   0x0101

void PushMsg()
{
	EnterCriticalSection(&cs_);
	msgQueue_.push(MSG_USER_IDLE);
	LeaveCriticalSection(&cs_);
}


 LRESULT CALLBACK MyKeyboardProc(
	int    code,
	WPARAM wParam,
	LPARAM lParam
	)
{
	 if (code < 0)
	 {
		 return CallNextHookEx(NULL, code, wParam, lParam);
	 }
	 else
	 {
		 PushMsg();
		 return CallNextHookEx(NULL, code, wParam, lParam);
	 }
	 
}

 LRESULT CALLBACK MyMouseProc(
	int    nCode,
	WPARAM wParam,
	LPARAM lParam
	)
{
	 if (nCode < 0)
	 {
		 return CallNextHookEx(NULL, nCode, wParam, lParam);
	 }
	 else
	 {
		 PushMsg();
		 return CallNextHookEx(NULL, nCode, wParam, lParam);
	 }
	 
}

 unsigned __stdcall msgProc(void* arg)
 {
	 int msg;
	 time_t current_time;
	 static bool first_msg = true;
	 while (!bQuit_)
	 {
		 EnterCriticalSection(&cs_);
		 if (msgQueue_.empty())
		 {
			 LeaveCriticalSection(&cs_);
			 Sleep(10);
			 continue;
		 }
		 msg = msgQueue_.front();
		 msgQueue_.pop();
		 LeaveCriticalSection(&cs_);
		 
		 time(&current_time);
		 if (last_receive_msg_time = 0)
			 last_receive_msg_time = current_time;

		 if (current_time - last_receive_msg_time > MAX_IDLE_TIME)
		 {
			 if (targetwindow_ != NULL)
			 {
				 ::PostMessage(targetwindow_, WM_DRAW_PICTURE, 0, 0);
			 }
		 }
		 last_receive_msg_time = current_time;
		 Sleep(10);
	 }
	 return 0;
 }


void init()
{
	targetwindow_ = ::FindWindow(NULL, "BootstrapWindow");
	InitializeCriticalSection(&cs_);
	HANDLE hthread = (HANDLE)_beginthreadex(NULL, 0, &msgProc, NULL, 0, NULL);
	CloseHandle(hthread);
}

void uninit()
{
	bQuit_ = true;
}