// wnd_hooks.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "wnd_hooks.h"


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


WND_HOOKS_API LRESULT CALLBACK myMsgHook(
	 int    code,
	 WPARAM wParam,
	 LPARAM lParam
	)
{
	LPMSG pMsg;
	unsigned short msg;
	if (code == HC_ACTION)
	{
		pMsg = (LPMSG)lParam;
		msg = pMsg->message && 0xffff;
		if (msg == WM_KEYDOWN)
		{

		}
	}
	else
	{
		return ::CallNextHookEx(NULL, code, wParam, lParam);
	}

}


void init()
{
	
}

void uninit()
{

}