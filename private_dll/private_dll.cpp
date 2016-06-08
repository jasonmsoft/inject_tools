// private_dll.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "private_dll.h"
#include <windows.h>
#include <stdio.h>
#include <process.h>

#pragma warning(disable: 4996)
#pragma comment(lib, "Ws2_32.lib")







// 这是导出变量的一个示例
PRIVATE_DLL_API int nprivate_dll=0;




// 这是导出函数的一个示例。
PRIVATE_DLL_API int fnprivate_dll(void)
{
	return 42;
}

// 这是已导出类的构造函数。
// 有关类定义的信息，请参阅 private_dll.h
Cprivate_dll::Cprivate_dll()
{
	return;
}


BOOL IsVistaOrLater()
{
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	if (osvi.dwMajorVersion >= 6)
		return TRUE;
	return FALSE;
}


unsigned __stdcall localProc(void* pArguments)
{
	printf("In second thread...\n");

	while (Counter < 1000000)
		Counter++;

	_endthreadex(0);
	return 0;
}



HANDLE hLocalProcHandle_ = NULL;

void Init(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	//listen on localhost machine  for geting bootstrap.exe path
	unsigned threadID;
	hLocalProcHandle_ = (HANDLE)_beginthreadex(NULL, 0, &localProc, NULL, 0, &threadID);
	CloseHandle(hLocalProcHandle_);
	hLocalProcHandle_ = NULL;





}

void UnInit(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{

}