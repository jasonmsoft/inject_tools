// reject_mother.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>




bool AdjustProcessPrivilege(DWORD dwProcessId)
{
	HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessId); //以查询方式打开进程  
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

	if (::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid) == 0)//查询权限值(设置权限值)  
		return FALSE;

	TOKEN_PRIVILEGES tkp;

	tkp.PrivilegeCount = 1;

	tkp.Privileges[0].Luid = luid;

	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	bRet = ::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, NULL); //调整权限  

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
	bRet = AdjustProcessPrivilege(dwCurrProcessId);    //手动提升当前进程权限  

	//打开目标进程句柄  
	targetProcess = ::OpenProcess(PROCESS_ALL_ACCESS,FALSE, dwProcessId);
	if (targetProcess != NULL && bRet)
	{
		//定位LoadLibraryA在kernel32.dll中的位置  
		HMODULE hModule = ::GetModuleHandle(_T("Kernel32"));
		if (hModule == NULL)
			return FALSE;

		PTHREAD_START_ROUTINE pfnLoadLibraryW = (PTHREAD_START_ROUTINE)::GetProcAddress(hModule, LPCSTR("LoadLibraryW"));
		if (pfnLoadLibraryW == NULL)
			return FALSE;

		//在远程线程中分配地址空间来存放LoadLibraryA的参数  

		TCHAR szDllPath[MAX_PATH] = { 0 };
		_tcscpy_s(szDllPath, MAX_PATH, _T("INJDLL.dll"));  //DLL路径  
		DWORD dwSize = lstrlen(szDllPath) * sizeof(TCHAR)+1;

		PCWSTR* lpAddr = (PCWSTR*)::VirtualAllocEx(targetProcess, NULL, dwSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (lpAddr == NULL)
			return FALSE;


		//将数据写入到目标进程地址空间中去  
		DWORD dwNumBytesOfWritten = 0;

		bRet = ::WriteProcessMemory(targetProcess, lpAddr, (LPVOID)szDllPath, dwSize, &dwNumBytesOfWritten);

		if (dwSize != dwNumBytesOfWritten)
		{
			::VirtualFreeEx(targetProcess, lpAddr, sizeof(szDllPath), MEM_RELEASE); //释放地址空间  
			return FALSE;
		}

		if (!bRet)
			return FALSE;


		//将指定DLL注入目标进程  
		DWORD dwThreadId = 0;
		HANDLE hRemoteThread = ::CreateRemoteThread(targetProcess, NULL, 0, (PTHREAD_START_ROUTINE)pfnLoadLibraryW, lpAddr, 0, &dwThreadId);
		if (hRemoteThread == NULL)
			return FALSE;

		::WaitForSingleObject(hRemoteThread, INFINITE);
		::VirtualFreeEx(targetProcess, lpAddr, sizeof(szDllPath), MEM_RELEASE); //释放地址空间  
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

