// private_dll.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "private_dll.h"


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





void Init(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{

}

void UnInit(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{

}