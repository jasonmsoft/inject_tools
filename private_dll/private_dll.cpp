// private_dll.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "private_dll.h"


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





void Init(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{

}

void UnInit(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{

}