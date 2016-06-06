// reject_mother.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <Windows.h>




bool AdjustProcessPrivilege(DWORD dwProcessId)
{
	HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessId); //�Բ�ѯ��ʽ�򿪽���  
	if (hProcess == NULL)
		return FALSE;

	HANDLE hToken = NULL;
	BOOL bRet = FALSE;
	DWORD dwErr = 0;
	bRet = ::OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
	dwErr = ::GetLastError();
	if (!bRet)
		return FALSE;

	LUID luid;

	if (::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid) == 0)//��ѯȨ��ֵ(����Ȩ��ֵ)  
		return FALSE;

	TOKEN_PRIVILEGES tkp;

	tkp.PrivilegeCount = 1;

	tkp.Privileges[0].Luid = luid;

	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	bRet = ::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, NULL); //����Ȩ��  

	if (!bRet)
		return FALSE;

	::CloseHandle(hToken);
	::CloseHandle(hProcess);
	return TRUE;
}



bool InjectDesProcess(DWORD dwProcessId)
{

	BOOL bRet = FALSE;
	HANDLE targetProcess;
	DWORD dwCurrProcessId = ::GetCurrentProcessId();
	bRet = AdjustProcessPrivilege(dwCurrProcessId);    //�ֶ�������ǰ����Ȩ��  

	//��Ŀ����̾��  
	targetProcess = ::OpenProcess(PROCESS_ALL_ACCESS,FALSE, dwProcessId);
	if (targetProcess != NULL && bRet)
	{
		//��λLoadLibraryA��kernel32.dll�е�λ��  
		HMODULE hModule = ::GetModuleHandle(_T("Kernel32"));
		if (hModule == NULL)
			return FALSE;

		PTHREAD_START_ROUTINE pfnLoadLibraryW = (PTHREAD_START_ROUTINE)::GetProcAddress(hModule, LPCSTR("LoadLibraryW"));
		if (pfnLoadLibraryW == NULL)
			return FALSE;

		//��Զ���߳��з����ַ�ռ������LoadLibraryA�Ĳ���  

		TCHAR szDllPath[MAX_PATH] = { 0 };
		_tcscpy_s(szDllPath, MAX_PATH, _T("INJDLL.dll"));  //DLL·��  
		DWORD dwSize = lstrlen(szDllPath) * sizeof(TCHAR)+1;

		PCWSTR* lpAddr = (PCWSTR*)::VirtualAllocEx(targetProcess, NULL, dwSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (lpAddr == NULL)
			return FALSE;


		//������д�뵽Ŀ����̵�ַ�ռ���ȥ  
		DWORD dwNumBytesOfWritten = 0;

		bRet = ::WriteProcessMemory(targetProcess, lpAddr, (LPVOID)szDllPath, dwSize, &dwNumBytesOfWritten);

		if (dwSize != dwNumBytesOfWritten)
		{
			::VirtualFreeEx(targetProcess, lpAddr, sizeof(szDllPath), MEM_RELEASE); //�ͷŵ�ַ�ռ�  
			return FALSE;
		}

		if (!bRet)
			return FALSE;


		//��ָ��DLLע��Ŀ�����  
		DWORD dwThreadId = 0;
		HANDLE hRemoteThread = ::CreateRemoteThread(targetProcess, NULL, 0, (PTHREAD_START_ROUTINE)pfnLoadLibraryW, lpAddr, 0, &dwThreadId);
		if (hRemoteThread == NULL)
			return FALSE;

		::WaitForSingleObject(hRemoteThread, INFINITE);
		::VirtualFreeEx(targetProcess, lpAddr, sizeof(szDllPath), MEM_RELEASE); //�ͷŵ�ַ�ռ�  
		::CloseHandle(hRemoteThread);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}



int _tmain(int argc, _TCHAR* argv[])
{
	return 0;
}

