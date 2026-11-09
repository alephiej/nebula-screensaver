/*
 * Windows screen saver shell for Nebula (nebula.scr).
 * Distributed under the terms of the MIT License.
 *
 *   /s            run full screen on every monitor
 *   /p <hwnd>     preview inside the small window of the settings dialog
 *   /c[:<hwnd>]   configuration dialog (also the default with no argument)
 */

#include <windows.h>
#include <mmsystem.h>

#include <stdlib.h>

#include "../src/nebula.h"
#include "settings.h"

#define CLASS_NAME "NebulaSaverWindow"
#define MOUSE_SLOP 8		/* pixels of cursor travel before we quit */
#define MAX_SAVER_WINDOWS 16

typedef struct {
	HWND hwnd;
	nebula* neb;
	HDC memdc;
	HBITMAP dib;
	HGDIOBJ old_bitmap;
	uint32_t* bits;
	int fw, fh;
	int repaint_background;
} saver_window;

static nebula_settings gSettings;
static saver_window gWindows[MAX_SAVER_WINDOWS];
static int gWindowCount;
static HINSTANCE gInstance;
static HWND gPreviewParent;
static int gPreview;
static POINT gInitialCursor;
static LARGE_INTEGER gFreq, gStart;


static saver_window*
find_window(HWND hwnd)
{
	int i;
	for (i = 0; i < gWindowCount; i++) {
		if (gWindows[i].hwnd == hwnd)
			return &gWindows[i];
	}
	return NULL;
}


static void
free_buffers(saver_window* w)
{
	if (w->memdc != NULL) {
		SelectObject(w->memdc, w->old_bitmap);
		DeleteDC(w->memdc);
	}
	if (w->dib != NULL)
		DeleteObject(w->dib);
	nebula_destroy(w->neb);
	w->memdc = NULL;
	w->dib = NULL;
	w->neb = NULL;
}


static int
setup_buffers(saver_window* w)
{
	RECT rc;
	BITMAPINFO bi;
	HDC screen;

	GetClientRect(w->hwnd, &rc);
	/* the preview pane is tiny: render at a normal size and scale it down */
	nebula_frame_size(gPreview && gSettings.width == 0 ? 640 : gSettings.width,
		gPreview ? 0 : gSettings.border, rc.right, rc.bottom, &w->fw, &w->fh);

	w->neb = nebula_create(w->fw, w->fh, GetTickCount() ^ (unsigned)(size_t)w);
	if (w->neb == NULL)
		return 0;
	w->neb->motion_blur = gSettings.blur;
	w->neb->speed = gSettings.speed;
	nebula_set_palette(w->neb, gSettings.scheme);

	ZeroMemory(&bi, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = w->fw;
	bi.bmiHeader.biHeight = -w->fh;		/* top-down */
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	screen = GetDC(w->hwnd);
	w->dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void**)&w->bits,
		NULL, 0);
	w->memdc = CreateCompatibleDC(screen);
	ReleaseDC(w->hwnd, screen);
	if (w->dib == NULL || w->memdc == NULL) {
		free_buffers(w);
		return 0;
	}
	w->old_bitmap = SelectObject(w->memdc, w->dib);
	w->repaint_background = 1;
	return 1;
}


static void
present(saver_window* w)
{
	RECT rc;
	int cw, ch, dw, dh;
	float scale;
	HDC dc;

	GetClientRect(w->hwnd, &rc);
	cw = rc.right;
	ch = rc.bottom;
	if (cw <= 0 || ch <= 0)
		return;

	/* letterbox: fit the frame, keep its aspect ratio */
	scale = (float)cw / w->fw;
	if ((float)ch / w->fh < scale)
		scale = (float)ch / w->fh;
	dw = (int)(w->fw * scale);
	dh = (int)(w->fh * scale);

	dc = GetDC(w->hwnd);
	if (w->repaint_background) {
		FillRect(dc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
		w->repaint_background = 0;
	}
	SetStretchBltMode(dc, COLORONCOLOR);	/* nearest neighbour */
	StretchBlt(dc, (cw - dw) / 2, (ch - dh) / 2, dw, dh, w->memdc, 0, 0,
		w->fw, w->fh, SRCCOPY);
	ReleaseDC(w->hwnd, dc);
}


static void
render_all(void)
{
	LARGE_INTEGER now;
	double t;
	int i;

	QueryPerformanceCounter(&now);
	t = (double)(now.QuadPart - gStart.QuadPart) / gFreq.QuadPart;

	for (i = 0; i < gWindowCount; i++) {
		saver_window* w = &gWindows[i];
		if (w->neb == NULL)
			continue;
		nebula_render(w->neb, t);
		nebula_blit_rgb(w->neb, w->bits, w->fw);
		present(w);
	}
}


static LRESULT CALLBACK
window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_ERASEBKGND:
			return 1;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			saver_window* w = find_window(hwnd);
			BeginPaint(hwnd, &ps);
			EndPaint(hwnd, &ps);
			if (w != NULL)
				w->repaint_background = 1;
			return 0;
		}

		case WM_SETCURSOR:
			if (!gPreview) {
				SetCursor(NULL);
				return TRUE;
			}
			break;

		case WM_SYSCOMMAND:
			/* no starting another saver or the monitor off on top of us */
			if (!gPreview && (wParam & 0xfff0) == SC_SCREENSAVE)
				return 0;
			break;

		case WM_ACTIVATEAPP:
			if (!gPreview && wParam == FALSE)
				PostQuitMessage(0);
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			if (!gPreview)
				PostQuitMessage(0);
			return 0;

		case WM_MOUSEMOVE:
			if (!gPreview) {
				POINT p;
				GetCursorPos(&p);
				if (abs(p.x - gInitialCursor.x) + abs(p.y - gInitialCursor.y)
						> MOUSE_SLOP)
					PostQuitMessage(0);
			}
			return 0;

		case WM_DESTROY:
			if (gPreview)
				PostQuitMessage(0);
			return 0;
	}
	return DefWindowProcA(hwnd, msg, wParam, lParam);
}


static int
create_window(const RECT* area, HWND parent)
{
	saver_window* w;
	HWND hwnd;

	if (gWindowCount >= MAX_SAVER_WINDOWS)
		return 0;

	if (parent != NULL) {
		hwnd = CreateWindowExA(0, CLASS_NAME, "Nebula", WS_CHILD | WS_VISIBLE,
			0, 0, area->right - area->left, area->bottom - area->top, parent,
			NULL, gInstance, NULL);
	} else {
		hwnd = CreateWindowExA(WS_EX_TOPMOST, CLASS_NAME, "Nebula",
			WS_POPUP | WS_VISIBLE, area->left, area->top,
			area->right - area->left, area->bottom - area->top, NULL, NULL,
			gInstance, NULL);
	}
	if (hwnd == NULL)
		return 0;

	w = &gWindows[gWindowCount];
	ZeroMemory(w, sizeof(*w));
	w->hwnd = hwnd;
	if (!setup_buffers(w)) {
		DestroyWindow(hwnd);
		return 0;
	}
	gWindowCount++;
	return 1;
}


static BOOL CALLBACK
monitor_proc(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM data)
{
	(void)monitor;
	(void)dc;
	(void)data;
	create_window(rect, NULL);
	return TRUE;
}


static int
run(void)
{
	MSG msg;
	DWORD budget = 1000 / (gSettings.fps > 0 ? gSettings.fps : 40);
	int i;

	QueryPerformanceFrequency(&gFreq);
	QueryPerformanceCounter(&gStart);
	timeBeginPeriod(1);

	for (;;) {
		DWORD frame_start = timeGetTime(), spent;

		while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				goto done;
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		if (gPreview && !IsWindow(gPreviewParent))
			break;

		render_all();

		spent = timeGetTime() - frame_start;
		if (spent < budget)
			MsgWaitForMultipleObjects(0, NULL, FALSE, budget - spent,
				QS_ALLINPUT);
	}

done:
	timeEndPeriod(1);
	for (i = 0; i < gWindowCount; i++) {
		free_buffers(&gWindows[i]);
		if (IsWindow(gWindows[i].hwnd))
			DestroyWindow(gWindows[i].hwnd);
	}
	return 0;
}


/* Parses "/s", "/p 123", "/p:123", "/c:123", case-insensitive, '-' allowed. */
static char
parse_command_line(const char* cmd, HWND* hwnd)
{
	char mode;

	*hwnd = NULL;
	while (*cmd == ' ' || *cmd == '\t')
		cmd++;
	if (*cmd != '/' && *cmd != '-')
		return 'c';
	cmd++;
	mode = *cmd >= 'A' && *cmd <= 'Z' ? *cmd + ('a' - 'A') : *cmd;
	if (mode != 0)
		cmd++;
	while (*cmd == ' ' || *cmd == ':' || *cmd == '\t')
		cmd++;
	if (*cmd >= '0' && *cmd <= '9')
		*hwnd = (HWND)(UINT_PTR)_strtoui64(cmd, NULL, 10);
	return mode == 0 ? 'c' : mode;
}


int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd, int show)
{
	WNDCLASSA wc;
	HWND arg;
	char mode;

	(void)prev;
	(void)show;
	gInstance = instance;
	mode = parse_command_line(cmd, &arg);

	settings_load(&gSettings);

	switch (mode) {
		case 'c':
			/* with no parent, fall back to the foreground window */
			config_dialog(instance, arg != NULL ? arg : GetForegroundWindow());
			return 0;

		case 'p':
		case 's':
			break;

		default:
			return 0;		/* /a (password change) and anything else */
	}

	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = window_proc;
	wc.hInstance = instance;
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = CLASS_NAME;
	if (!RegisterClassA(&wc))
		return 1;

	if (mode == 'p') {
		RECT rc;
		if (arg == NULL || !IsWindow(arg))
			return 0;
		gPreview = 1;
		gPreviewParent = arg;
		GetClientRect(arg, &rc);
		if (!create_window(&rc, arg))
			return 1;
	} else {
		GetCursorPos(&gInitialCursor);
		EnumDisplayMonitors(NULL, NULL, monitor_proc, 0);
		if (gWindowCount == 0)
			return 1;
		ShowCursor(FALSE);
	}
	return run();
}
