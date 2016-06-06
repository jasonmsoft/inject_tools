// reject_mother.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <vector>
#include <TlHelp32.h>
using namespace std;


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



bool InjectDesProcess(DWORD dwProcessId ,TCHAR *dllPathName)
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
		_tcscpy_s(szDllPath, MAX_PATH, dllPathName);  //DLL路径  
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


vector<DWORD> GetProcessIDByName(TCHAR * processName)
{
	vector<DWORD> processIDs;

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (strcmp(entry.szExeFile, processName) == 0)
			{
				processIDs.push_back(entry.th32ProcessID);
			}
		}
	}
	CloseHandle(snapshot);

	return processIDs;
}


int _tmain(int argc, _TCHAR* argv[])
{
	// <dllpathname> <target_process_name>
	TCHAR dllPathName[255] = { 0 };
	TCHAR targetProcName[255] = { 0 };
	vector<DWORD> targetPids;
	if (argc != 3)
	{
		exit(-1);
	}
	strcpy(dllPathName, argv[1]);
	strcpy(targetProcName, argv[2]);

	targetPids = GetProcessIDByName(targetProcName);

	for (vector<DWORD>::iterator it = targetPids.begin(); it != targetPids.end(); it++)
	{
		printf("targetProcName %s id is %u \n", targetProcName, *it);
	}

	system("pause");
	return 0;
}

