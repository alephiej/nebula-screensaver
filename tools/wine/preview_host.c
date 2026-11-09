#include <windows.h>
#include <stdio.h>
int WINAPI WinMain(HINSTANCE i, HINSTANCE p, LPSTR c, int s)
{
	HWND w = CreateWindowA("STATIC", "preview host", WS_OVERLAPPED | WS_VISIBLE | WS_CLIPCHILDREN,
		100, 100, 152, 112, NULL, NULL, i, NULL);
	char cmd[256];
	STARTUPINFOA si = {sizeof(si)};
	PROCESS_INFORMATION pi;
	RECT rc;
	GetClientRect(w, &rc);
	sprintf(cmd, "nebula.scr /p %llu", (unsigned long long)(size_t)w);
	CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	{
		DWORD end = GetTickCount() + 7000;
		MSG m;
		while ((int)(end - GetTickCount()) > 0) {
			while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&m);
				DispatchMessageA(&m);
			}
			MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLINPUT);
		}
	}
	DestroyWindow(w);
	/* the saver must notice the parent is gone and exit */
	return WaitForSingleObject(pi.hProcess, 5000) == WAIT_OBJECT_0 ? 0 : 3;
}
