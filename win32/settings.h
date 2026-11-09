#ifndef NEBULA_SETTINGS_H
#define NEBULA_SETTINGS_H

typedef struct {
	int width;			/* internal width, 0 = native */
	int scheme;			/* 0..NEBULA_PALETTE_SCHEMES-1 */
	int blur;
	float speed;
	int fps;
	int border;			/* 0..3 */
} nebula_settings;

void settings_defaults(nebula_settings* s);
void settings_load(nebula_settings* s);		/* HKCU\Software\nebula-screensaver */
void settings_save(const nebula_settings* s);

/* modal dialog; returns 1 if the user accepted (and settings were saved) */
int config_dialog(void* hinstance, void* parent);

#endif
