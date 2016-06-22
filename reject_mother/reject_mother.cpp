// reject_mother.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <vector>
#include <TlHelp32.h>
#include <iostream>
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

typedef struct dll_container
{
	string dll_name;
	string dll_path;
	DWORD pid;
	BYTE *mod_base_addr;
}DLL_CONTAINER_T;


bool UnInjectDesProcess(DLL_CONTAINER_T *dll)
{

	bool bRet = FALSE;
	HANDLE targetProcess;
	DWORD dwCurrProcessId = ::GetCurrentProcessId();
	bRet = AdjustProcessPrivilege(dwCurrProcessId);    //手动提升当前进程权限  

	//打开目标进程句柄  
	targetProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, dll->pid);
	if (targetProcess != NULL && bRet)
	{
		//定位LoadLibraryA在kernel32.dll中的位置  
		HMODULE hModule = ::GetModuleHandle(_T("Kernel32"));
		if (hModule == NULL)
		{
			printf("get kernel32 failed\n");
			return FALSE;
		}


		int cByte = (dll->dll_name.length() + 1) * sizeof(char);
		LPVOID pAddr = VirtualAllocEx(targetProcess, NULL, cByte, MEM_COMMIT, PAGE_READWRITE);
		SIZE_T written = 0;
		if (!pAddr || !WriteProcessMemory(targetProcess, pAddr, dll->dll_name.c_str(), cByte, &written)) {
			printf("WriteProcessMemory failed\n");
			return FALSE;
		}

		if(written != cByte)
		{
			printf("write dll  name to remote process failed \n");
			return FALSE;
		}

#ifdef _UNICODE  
		PTHREAD_START_ROUTINE pfnGetModuleHandle = (PTHREAD_START_ROUTINE)GetModuleHandleW;
#else  
		PTHREAD_START_ROUTINE pfnGetModuleHandle = (PTHREAD_START_ROUTINE)GetModuleHandleA;
#endif  
		//Kernel32.dll总是被映射到相同的地址  
		if (!pfnGetModuleHandle) {
			printf("pfnGetModuleHandle failed\n");
			return FALSE;
		}
		DWORD dwThreadID = 0, dwFreeId = 0, dwHandle;
		HANDLE hRemoteThread = CreateRemoteThread(targetProcess, NULL, 0, pfnGetModuleHandle, pAddr, 0, &dwThreadID);
		if (!hRemoteThread) {
			printf("hRemoteThread failed\n");
			return FALSE;
		}
		WaitForSingleObject(hRemoteThread, INFINITE);
		// 获得GetModuleHandle的返回值  
		GetExitCodeThread(hRemoteThread, &dwHandle);
		CloseHandle(hRemoteThread);




		PTHREAD_START_ROUTINE pfnFreeLibrary = (PTHREAD_START_ROUTINE)::GetProcAddress(hModule, LPCSTR("FreeLibrary"));
		if (pfnFreeLibrary == NULL)
		{
			printf("get FreeLibraryAndExitThread failed\n");
			return FALSE;
		}

		INT64 lHandle = dwHandle;

		printf("freelibrary handle 0x%x size %d\n", lHandle, sizeof(lHandle));
		

		LPVOID pAddrHandle = VirtualAllocEx(targetProcess, NULL, sizeof(lHandle), MEM_COMMIT, PAGE_READWRITE);
		if (!pAddrHandle || !WriteProcessMemory(targetProcess, pAddrHandle, &lHandle, sizeof(lHandle), &written)) {
			printf("WriteProcessMemory failed for handle\n");
			return FALSE;
		}
		if (written != sizeof(lHandle))
		{
			printf("write dll handle data failed");
			return FALSE;
		}
		INT64 handle;
		if (ReadProcessMemory(
			targetProcess,
			pAddrHandle,
			&handle,
			sizeof(handle),
			&written
			))
		{
			printf("read value is 0x%x dll base addr 0x%x\n", handle, dll->mod_base_addr);
		}

		//将指定DLL从目标进程卸载  
		DWORD dwThreadId = 0;
		hRemoteThread = ::CreateRemoteThread(targetProcess, NULL, 0, (PTHREAD_START_ROUTINE)pfnFreeLibrary, (LPVOID)dll->mod_base_addr, 0, &dwThreadId);
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
					pfnFreeLibrary,
					(LPVOID)dwHandle,
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
		GetExitCodeThread(hRemoteThread, &dwHandle);
		printf("free dll ret %d \n", dwHandle);
		if (dwHandle == 0)
		{
			PTHREAD_START_ROUTINE pfnGetLastError = (PTHREAD_START_ROUTINE)::GetProcAddress(hModule, LPCSTR("GetLastError"));
			hRemoteThread = ::CreateRemoteThread(targetProcess, NULL, 0, (PTHREAD_START_ROUTINE)pfnGetLastError, (LPVOID)NULL, 0, &dwThreadId);
			::WaitForSingleObject(hRemoteThread, INFINITE);
			GetExitCodeThread(hRemoteThread, &dwHandle);
			printf("error code is %d \n", dwHandle);
		}
		VirtualFreeEx(targetProcess, pAddrHandle, sizeof(lHandle), MEM_COMMIT);
		VirtualFreeEx(targetProcess, pAddr, cByte, MEM_COMMIT); 
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





//遍历进程的DLL
vector<DLL_CONTAINER_T> traverseModels(const std::string process_name)
{
	DWORD dwId;
	vector<DLL_CONTAINER_T> dll_vec;

	vector<DWORD> pids = GetProcessIDByName((char *)process_name.c_str());

	for (vector<DWORD>::iterator it = pids.begin(); it != pids.end(); it++)
	{
		dwId = *it;
		printf("process id : %u \n", dwId);

		HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwId);
		if (hModuleSnap == INVALID_HANDLE_VALUE){
			printf("CreateToolhelp32SnapshotError! \n");
			return dll_vec;
		}
		MODULEENTRY32 module32;
		module32.dwSize = sizeof(module32);
		BOOL bResult = Module32First(hModuleSnap, &module32);
		while (bResult){
			DLL_CONTAINER_T dll;
			dll.dll_name = module32.szModule;
			dll.dll_path = module32.szExePath;
			dll.pid = dwId;
			dll.mod_base_addr = module32.modBaseAddr;
			dll_vec.push_back(dll);
			bResult = Module32Next(hModuleSnap, &module32);
		}
		CloseHandle(hModuleSnap);
	}

	return dll_vec;

}


void getDllName(char * path, int path_len,  char *name)
{
	for (int i = path_len - 1; i > 0; i--)
	{
		if (path[i] == '\\')
		{
			strcpy(name, &path[i + 1]);
			break;
		}
	}
}


void unloadDll(DLL_CONTAINER_T *dll)
{
	bool ret = false;
	ret = UnInjectDesProcess(dll);
	if (ret)
	{
		printf("uninject success\n");
	}
	else
	{
		printf("uninject failed \n");
	}
	
}


int _tmain(int argc, _TCHAR* argv[])
{
	// <dllpathname> <target_process_name>  inject dll into target process
	// -u <dllName>  <target_process_name>  Uninject dll from target process
#define ACTION_INJECT 1
#define ACTION_UNINJECT 2

	TCHAR dllPathName[255] = { 0 };
	TCHAR dllName[255] = { 0 };
	TCHAR targetProcName[255] = { 0 };
	vector<DWORD> targetPids;
	bool ret = false;
	int action = ACTION_INJECT;

	if (argc == 3)
	{
		action = ACTION_INJECT;
		strcpy(dllPathName, argv[1]);
		getDllName(dllPathName, strlen(dllPathName), dllName);
		strcpy(targetProcName, argv[2]);
	}
	else if (argc == 4 && strcmp(argv[1], "-u") == 0)
	{
		action = ACTION_UNINJECT;
		strcpy(dllName, argv[2]);
		strcpy(targetProcName, argv[3]);
	}
	else
	{
		exit(-1);
	}
	targetPids = GetProcessIDByName(targetProcName);
	
	

	if (action == ACTION_INJECT)
	{
		for (vector<DWORD>::iterator it = targetPids.begin(); it != targetPids.end(); it++)
		{
			ret = InjectDesProcess(*it, dllPathName);
			if (ret)
			{
				printf("inject %s into pid %d success!\n", dllPathName, *it);
			}
		}
	}
	else if (action == ACTION_UNINJECT)
	{
		vector<DLL_CONTAINER_T> dlls = traverseModels(targetProcName);
		BOOL hasdll = FALSE;

		DLL_CONTAINER_T dll;

		for (vector<DLL_CONTAINER_T>::iterator it = dlls.begin(); it != dlls.end(); it++)
		{
			if (it->dll_name == dllName)
			{
				printf("dll : %s exists\n", it->dll_name.c_str());
				hasdll = TRUE;
				dll.dll_name = it->dll_name;
				dll.dll_path = it->dll_path;
				dll.mod_base_addr = it->mod_base_addr;
				dll.pid = it->pid;
				
				unloadDll(&dll);
			}
			
		}
	}


	
	system("pause");
	return 0;
}

