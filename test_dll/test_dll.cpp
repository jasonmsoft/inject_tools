// test_dll.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "test_dll.h"


// ���ǵ���������һ��ʾ��
TEST_DLL_API int ntest_dll=0;

// ���ǵ���������һ��ʾ����
TEST_DLL_API int fntest_dll(void)
{
	return 42;
}

// �����ѵ�����Ĺ��캯����
// �й��ඨ�����Ϣ������� test_dll.h
Ctest_dll::Ctest_dll()
{
	return;
}
