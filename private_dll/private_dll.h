// ���� ifdef ���Ǵ���ʹ�� DLL �������򵥵�
// ��ı�׼�������� DLL �е������ļ��������������϶���� PRIVATE_DLL_EXPORTS
// ���ű���ġ���ʹ�ô� DLL ��
// �κ�������Ŀ�ϲ�Ӧ����˷��š�������Դ�ļ��а������ļ����κ�������Ŀ���Ὣ
// PRIVATE_DLL_API ������Ϊ�Ǵ� DLL ����ģ����� DLL ���ô˺궨���
// ������Ϊ�Ǳ������ġ�
#ifdef PRIVATE_DLL_EXPORTS
#define PRIVATE_DLL_API __declspec(dllexport)
#else
#define PRIVATE_DLL_API __declspec(dllimport)
#endif

// �����Ǵ� private_dll.dll ������
class PRIVATE_DLL_API Cprivate_dll {
public:
	Cprivate_dll(void);
	// TODO:  �ڴ�������ķ�����
};

extern PRIVATE_DLL_API int nprivate_dll;

PRIVATE_DLL_API int fnprivate_dll(void);



void Init(HMODULE hModule,DWORD  ul_reason_for_call,LPVOID lpReserved);

void UnInit(HMODULE hModule,DWORD  ul_reason_for_call,LPVOID lpReserved);
