// wnd_hooks.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "wnd_hooks.h"


// ���ǵ���������һ��ʾ��
WND_HOOKS_API int nwnd_hooks=0;

// ���ǵ���������һ��ʾ����
WND_HOOKS_API int fnwnd_hooks(void)
{
	return 42;
}


// �����ѵ�����Ĺ��캯����
// �й��ඨ�����Ϣ������� wnd_hooks.h
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