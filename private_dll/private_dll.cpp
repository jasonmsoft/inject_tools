// private_dll.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "private_dll.h"
#include <windows.h>
#include <stdio.h>
#include <process.h>

#pragma warning(disable: 4996)
#pragma comment(lib, "Ws2_32.lib")







// ���ǵ���������һ��ʾ��
PRIVATE_DLL_API int nprivate_dll=0;




// ���ǵ���������һ��ʾ����
PRIVATE_DLL_API int fnprivate_dll(void)
{
	return 42;
}

// �����ѵ�����Ĺ��캯����
// �й��ඨ�����Ϣ������� private_dll.h
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