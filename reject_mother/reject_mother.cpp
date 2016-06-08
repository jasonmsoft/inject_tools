// reject_mother.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <vector>
#include <TlHelp32.h>
using namespace std;
#pragma warning(disable: 4996)


bool AdjustProcessPrivilege(DWORD dwProcessId)
{
	HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessId); //以查询方式打开进程  
	if (hProcess == NULL)
	{
		printf("open process failed \n");
		return false;
	}
		

	HANDLE hToken = NULL;
	bool bRet = false;
	DWORD dwErr = 0;
	bRet = ::OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
	dwErr = ::GetLastError();
	if (!bRet)
	{
		printf(" OpenProcessToken failed %d\n", dwErr);
		return true;
	}
		

	LUID luid;

	if (::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid) == 0)//查询权限值(设置权限值)  
	{
		printf(" LookupPrivilegeValue failed \n");
		return false;
	}
		

	TOKEN_PRIVILEGES tkp;

	tkp.PrivilegeCount = 1;

	tkp.Privileges[0].Luid = luid;

	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	bRet = ::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, NULL); //调整权限  

	if (!bRet)
	{
		printf(" AdjustTokenPrivileges failed \n");
		return false;
	}
		

	::CloseHandle(hToken);
	::CloseHandle(hProcess);
	return true;
}

typedef DWORD(WINAPI *PFNTCREATETHREADEX)
(
PHANDLE                 ThreadHandle,
ACCESS_MASK             DesiredAccess,
LPVOID                  ObjectAttributes,
HANDLE                  ProcessHandle,
LPTHREAD_START_ROUTINE  lpStartAddress,
LPVOID                  lpParameter,
BOOL                    CreateSuspended,
DWORD                   dwStackSize,
DWORD                   dw1,
DWORD                   dw2,
LPVOID                  Unknown
);


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


bool InjectDesProcess(DWORD dwProcessId ,TCHAR *dllPathName)
{

	bool bRet = FALSE;
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
		{
			printf("get kernel32 failed\n");
			return FALSE;
		}
			

		PTHREAD_START_ROUTINE pfnLoadLibraryW = (PTHREAD_START_ROUTINE)::GetProcAddress(hModule, LPCSTR("LoadLibraryA"));
		if (pfnLoadLibraryW == NULL)
		{
			printf("get LoadLibraryW failed\n");
			return FALSE;
		}
			

		//在远程线程中分配地址空间来存放LoadLibraryA的参数  

		TCHAR szDllPath[MAX_PATH] = { 0 };
		_tcscpy_s(szDllPath, MAX_PATH, dllPathName);  //DLL路径  
		DWORD dwSize = lstrlen(szDllPath) * sizeof(TCHAR)+1;

		PCWSTR* lpAddr = (PCWSTR*)::VirtualAllocEx(targetProcess, NULL, dwSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (lpAddr == NULL)
		{
			printf("VirtualAllocEx failed\n");
			return FALSE;
		}
			


		//将数据写入到目标进程地址空间中去  
		SIZE_T dwNumBytesOfWritten = 0;

		bRet = ::WriteProcessMemory(targetProcess, lpAddr, (LPVOID)szDllPath, dwSize, &dwNumBytesOfWritten);
		if (dwSize != dwNumBytesOfWritten)
		{
			printf("WriteProcessMemory failed\n");
			::VirtualFreeEx(targetProcess, lpAddr, sizeof(szDllPath), MEM_RELEASE); //释放地址空间  
			return FALSE;
		}

		printf("write dll path: %s \n", szDllPath);

		if (!bRet)
		{
			printf("WriteProcessMemory failed\n");
			return FALSE;
		}
			


		//将指定DLL注入目标进程  
		DWORD dwThreadId = 0;
		HANDLE hRemoteThread = ::CreateRemoteThread(targetProcess, NULL, 0, (PTHREAD_START_ROUTINE)pfnLoadLibraryW, lpAddr, 0, &dwThreadId);
		if (hRemoteThread == NULL)
		{
			printf("CreateRemoteThread failed %d \n", GetLastError());
			if (IsVistaOrLater())
			{
				void * pFunc = NULL;
				pFunc = GetProcAddress(GetModuleHandle("ntdll.dll"), "NtCreateThreadEx");

				//下面就是用地址执行了NtCreateThreadEx  
				((PFNTCREATETHREADEX)pFunc)(
					&hRemoteThread,
					0x1FFFFF,
					NULL,
					targetProcess,
					pfnLoadLibraryW,
					lpAddr,
					FALSE,
					NULL,
					NULL,
					NULL,
					NULL);

				if (hRemoteThread == NULL)
				{
					printf("ntcreate remote thread failed %d\n", GetLastError());
					return FALSE;
				}
			}
			else
			{
				return FALSE;
			}
		}
			

		::WaitForSingleObject(hRemoteThread, INFINITE);
		::VirtualFreeEx(targetProcess, lpAddr, sizeof(szDllPath), MEM_RELEASE); //释放地址空间  
		::CloseHandle(hRemoteThread);
		return TRUE;
	}
	else
	{
		if (targetProcess == NULL)
		{
			printf("open target process failed  %d\n", GetLastError());
		}
		else
		{
			printf("privilege failed \n");
		}
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
	bool ret = false;
	if (argc != 3)
	{
		exit(-1);
	}



	strcpy(dllPathName, argv[1]);
	strcpy(targetProcName, argv[2]);

	targetPids = GetProcessIDByName(targetProcName);
	DWORD targetPid;

	for (vector<DWORD>::iterator it = targetPids.begin(); it != targetPids.end(); it++)
	{
		printf("targetProcName %s id is %u \n", targetProcName, *it);
		targetPid = *it;
	}


	ret = InjectDesProcess(targetPid, dllPathName);
	if (ret)
	{
		printf("inject success\n");
	}
	else
	{
		printf("inject failed \n");
	}
	system("pause");
	return 0;
}

