// ���� ifdef ���Ǵ���ʹ�� DLL �������򵥵�
// ��ı�׼�������� DLL �е������ļ��������������϶���� WND_HOOKS_EXPORTS
// ���ű���ġ���ʹ�ô� DLL ��
// �κ�������Ŀ�ϲ�Ӧ����˷��š�������Դ�ļ��а������ļ����κ�������Ŀ���Ὣ
// WND_HOOKS_API ������Ϊ�Ǵ� DLL ����ģ����� DLL ���ô˺궨���
// ������Ϊ�Ǳ������ġ�
#ifdef WND_HOOKS_EXPORTS
#define WND_HOOKS_API __declspec(dllexport)
#else
#define WND_HOOKS_API __declspec(dllimport)
#endif

// �����Ǵ� wnd_hooks.dll ������
class WND_HOOKS_API Cwnd_hooks {
public:
	Cwnd_hooks(void);
	// TODO:  �ڴ�������ķ�����
};

extern WND_HOOKS_API int nwnd_hooks;

WND_HOOKS_API int fnwnd_hooks(void);

void init();
void uninit();
WND_HOOKS_API LRESULT CALLBACK myMsgHook(int code, WPARAM wParam, LPARAM lParam);