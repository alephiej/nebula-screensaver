/*
 * Settings persistence and configuration dialog for the Windows screen saver.
 * Distributed under the terms of the MIT License.
 */

#include <windows.h>
#include <commctrl.h>

#include "../src/nebula.h"
#include "resource.h"
#include "settings.h"

#define REG_KEY "Software\\nebula-screensaver"

/* The Haiku slider maps 1..100 to speed 0.002 + 0.05 * value */
#define SPEED_MIN 0.002f
#define SPEED_STEP 0.05f

static const int kWidths[] = {0, 320, 512, 576, 640, 800, 1024, 1152, 1280,
	1400, 1600};
static const char* const kSchemes[NEBULA_PALETTE_SCHEMES] = {"yellow", "cyan",
	"red", "green", "grey", "cold", "orange (original)"};
static const char* const kBorders[] = {"fullscreen, no borders",
	"16:9, wide-screen", "2:3.5, cinemascope", "only a slit"};

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))


void
settings_defaults(nebula_settings* s)
{
	s->width = 640;
	s->scheme = 0;
	s->blur = 1;
	s->speed = 0.4f;
	s->fps = 40;
	s->border = 0;
}


static int
read_dword(HKEY key, const char* name, int* out)
{
	DWORD v, size = sizeof(v), type;
	if (RegQueryValueExA(key, name, NULL, &type, (BYTE*)&v, &size)
			== ERROR_SUCCESS && type == REG_DWORD) {
		*out = (int)v;
		return 1;
	}
	return 0;
}


static int
clampi(int v, int lo, int hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}


void
settings_load(nebula_settings* s)
{
	HKEY key;
	int v;

	settings_defaults(s);
	if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &key)
			!= ERROR_SUCCESS)
		return;

	if (read_dword(key, "width", &v))
		s->width = clampi(v, 0, 16384);
	if (read_dword(key, "scheme", &v))
		s->scheme = clampi(v, 0, NEBULA_PALETTE_SCHEMES - 1);
	if (read_dword(key, "blur", &v))
		s->blur = v != 0;
	if (read_dword(key, "speed_milli", &v))
		s->speed = clampi(v, 10, 10000) / 1000.0f;
	if (read_dword(key, "fps", &v))
		s->fps = clampi(v, 1, 240);
	if (read_dword(key, "border", &v))
		s->border = clampi(v, 0, 3);
	RegCloseKey(key);
}


static void
write_dword(HKEY key, const char* name, int v)
{
	DWORD d = (DWORD)v;
	RegSetValueExA(key, name, 0, REG_DWORD, (const BYTE*)&d, sizeof(d));
}


void
settings_save(const nebula_settings* s)
{
	HKEY key;
	if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, KEY_WRITE, NULL,
			&key, NULL) != ERROR_SUCCESS)
		return;

	write_dword(key, "width", s->width);
	write_dword(key, "scheme", s->scheme);
	write_dword(key, "blur", s->blur);
	write_dword(key, "speed_milli", (int)(s->speed * 1000 + 0.5f));
	write_dword(key, "fps", s->fps);
	write_dword(key, "border", s->border);
	RegCloseKey(key);
}


static void
update_labels(HWND dlg)
{
	char text[32];
	int speed = (int)SendDlgItemMessageA(dlg, IDC_SPEED, TBM_GETPOS, 0, 0);
	int fps = (int)SendDlgItemMessageA(dlg, IDC_FPS, TBM_GETPOS, 0, 0);
	int hundredths = (int)((SPEED_MIN + SPEED_STEP * speed) * 100 + 0.5f);

	wsprintfA(text, "%d.%02d", hundredths / 100, hundredths % 100);
	SetDlgItemTextA(dlg, IDC_SPEED_VAL, text);
	wsprintfA(text, "%d", fps);
	SetDlgItemTextA(dlg, IDC_FPS_VAL, text);
}


static void
fill_dialog(HWND dlg, const nebula_settings* s)
{
	char text[48];
	int i, sel = 0, speed;

	for (i = 0; i < COUNT(kWidths); i++) {
		if (kWidths[i] == 0)
			lstrcpyA(text, "screen resolution");
		else
			wsprintfA(text, "%d pixels", kWidths[i]);
		SendDlgItemMessageA(dlg, IDC_WIDTH, CB_ADDSTRING, 0, (LPARAM)text);
		if (kWidths[i] == s->width)
			sel = i;
	}
	SendDlgItemMessageA(dlg, IDC_WIDTH, CB_SETCURSEL, sel, 0);

	for (i = 0; i < NEBULA_PALETTE_SCHEMES; i++)
		SendDlgItemMessageA(dlg, IDC_SCHEME, CB_ADDSTRING, 0, (LPARAM)kSchemes[i]);
	SendDlgItemMessageA(dlg, IDC_SCHEME, CB_SETCURSEL, s->scheme, 0);

	for (i = 0; i < COUNT(kBorders); i++)
		SendDlgItemMessageA(dlg, IDC_BORDER, CB_ADDSTRING, 0, (LPARAM)kBorders[i]);
	SendDlgItemMessageA(dlg, IDC_BORDER, CB_SETCURSEL, s->border, 0);

	CheckDlgButton(dlg, IDC_BLUR, s->blur ? BST_CHECKED : BST_UNCHECKED);

	speed = clampi((int)((s->speed - SPEED_MIN) / SPEED_STEP + 0.5f), 1, 100);
	SendDlgItemMessageA(dlg, IDC_SPEED, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
	SendDlgItemMessageA(dlg, IDC_SPEED, TBM_SETTICFREQ, 10, 0);
	SendDlgItemMessageA(dlg, IDC_SPEED, TBM_SETPOS, TRUE, speed);
	SendDlgItemMessageA(dlg, IDC_FPS, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
	SendDlgItemMessageA(dlg, IDC_FPS, TBM_SETTICFREQ, 10, 0);
	SendDlgItemMessageA(dlg, IDC_FPS, TBM_SETPOS, TRUE, clampi(s->fps, 1, 100));
	update_labels(dlg);
}


static void
read_dialog(HWND dlg, nebula_settings* s)
{
	int sel = (int)SendDlgItemMessageA(dlg, IDC_WIDTH, CB_GETCURSEL, 0, 0);
	if (sel >= 0 && sel < COUNT(kWidths))
		s->width = kWidths[sel];
	sel = (int)SendDlgItemMessageA(dlg, IDC_SCHEME, CB_GETCURSEL, 0, 0);
	s->scheme = clampi(sel, 0, NEBULA_PALETTE_SCHEMES - 1);
	sel = (int)SendDlgItemMessageA(dlg, IDC_BORDER, CB_GETCURSEL, 0, 0);
	s->border = clampi(sel, 0, COUNT(kBorders) - 1);
	s->blur = IsDlgButtonChecked(dlg, IDC_BLUR) == BST_CHECKED;
	s->speed = SPEED_MIN + SPEED_STEP
		* (int)SendDlgItemMessageA(dlg, IDC_SPEED, TBM_GETPOS, 0, 0);
	s->fps = (int)SendDlgItemMessageA(dlg, IDC_FPS, TBM_GETPOS, 0, 0);
}


static INT_PTR CALLBACK
config_proc(HWND dlg, UINT msg, WPARAM wParam, LPARAM unused)
{
	(void)unused;
	switch (msg) {
		case WM_INITDIALOG:
		{
			nebula_settings s;
			settings_load(&s);
			fill_dialog(dlg, &s);
			return TRUE;
		}

		case WM_HSCROLL:
			update_labels(dlg);
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK) {
				nebula_settings s;
				read_dialog(dlg, &s);
				settings_save(&s);
				EndDialog(dlg, 1);
				return TRUE;
			}
			if (LOWORD(wParam) == IDCANCEL) {
				EndDialog(dlg, 0);
				return TRUE;
			}
			break;
	}
	return FALSE;
}


int
config_dialog(void* hinstance, void* parent)
{
	INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES};
	InitCommonControlsEx(&icc);
	return DialogBoxParamA((HINSTANCE)hinstance, MAKEINTRESOURCEA(IDD_CONFIG),
		(HWND)parent, config_proc, 0) == 1;
}
