// reject_mother.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <Windows.h>
#include <vector>
#include <TlHelp32.h>
using namespace std;
#pragma warning(disable: 4996)


bool AdjustProcessPrivilege(DWORD dwProcessId)
{
	HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessId); //�Բ�ѯ��ʽ�򿪽���  
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

	if (::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid) == 0)//��ѯȨ��ֵ(����Ȩ��ֵ)  
	{
		printf(" LookupPrivilegeValue failed \n");
		return false;
	}
		

	TOKEN_PRIVILEGES tkp;

	tkp.PrivilegeCount = 1;

	tkp.Privileges[0].Luid = luid;

	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	bRet = ::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, NULL); //����Ȩ��  

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
	bRet = AdjustProcessPrivilege(dwCurrProcessId);    //�ֶ�������ǰ����Ȩ��  

	//��Ŀ����̾��  
	targetProcess = ::OpenProcess(PROCESS_ALL_ACCESS,FALSE, dwProcessId);
	if (targetProcess != NULL && bRet)
	{
		//��λLoadLibraryA��kernel32.dll�е�λ��  
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
			

		//��Զ���߳��з����ַ�ռ������LoadLibraryA�Ĳ���  

		TCHAR szDllPath[MAX_PATH] = { 0 };
		_tcscpy_s(szDllPath, MAX_PATH, dllPathName);  //DLL·��  
		DWORD dwSize = lstrlen(szDllPath) * sizeof(TCHAR)+1;

		PCWSTR* lpAddr = (PCWSTR*)::VirtualAllocEx(targetProcess, NULL, dwSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (lpAddr == NULL)
		{
			printf("VirtualAllocEx failed\n");
			return FALSE;
		}
			


		//������д�뵽Ŀ����̵�ַ�ռ���ȥ  
		SIZE_T dwNumBytesOfWritten = 0;

		bRet = ::WriteProcessMemory(targetProcess, lpAddr, (LPVOID)szDllPath, dwSize, &dwNumBytesOfWritten);
		if (dwSize != dwNumBytesOfWritten)
		{
			printf("WriteProcessMemory failed\n");
			::VirtualFreeEx(targetProcess, lpAddr, sizeof(szDllPath), MEM_RELEASE); //�ͷŵ�ַ�ռ�  
			return FALSE;
		}

		printf("write dll path: %s \n", szDllPath);

		if (!bRet)
		{
			printf("WriteProcessMemory failed\n");
			return FALSE;
		}
			


		//��ָ��DLLע��Ŀ�����  
		DWORD dwThreadId = 0;
		HANDLE hRemoteThread = ::CreateRemoteThread(targetProcess, NULL, 0, (PTHREAD_START_ROUTINE)pfnLoadLibraryW, lpAddr, 0, &dwThreadId);
		if (hRemoteThread == NULL)
		{
			printf("CreateRemoteThread failed %d \n", GetLastError());
			if (IsVistaOrLater())
			{
				void * pFunc = NULL;
				pFunc = GetProcAddress(GetModuleHandle("ntdll.dll"), "NtCreateThreadEx");

				//��������õ�ִַ����NtCreateThreadEx  
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
		::VirtualFreeEx(targetProcess, lpAddr, sizeof(szDllPath), MEM_RELEASE); //�ͷŵ�ַ�ռ�  
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

