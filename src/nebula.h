/*
 * Nebula: software-rendered rotating galaxy.
 *
 * Portable C core, no platform dependencies. Ported from the Haiku/Vitruvian
 * Nebula screen saver (Axel Dörfler, Augustin Cavalier; effect from
 * corTeX / Optimum). Distributed under the terms of the MIT License.
 */
#ifndef NEBULA_H
#define NEBULA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEBULA_STARS 5000
#define NEBULA_PALETTE_SCHEMES 7

typedef struct {
	int x, y, z, r;
} nebula_star;

typedef struct nebula {
	int width, height;
	int motion_blur;
	float speed;				/* animation speed factor, 0.4 by default */
	uint32_t palette[256];		/* 0x00RRGGBB */

	nebula_star stars[NEBULA_STARS];
	float precos[512], presin[512];

	uint8_t* storage;			/* guard bytes + 8 bit frame + guard bytes */
	uint8_t* frame;				/* width * height 8 bit intensity buffer */
} nebula;

/* Returns NULL on allocation failure. seed makes the galaxy reproducible. */
nebula* nebula_create(int width, int height, unsigned seed);
void nebula_destroy(nebula* n);

/* scheme: 0 yellow, 1 cyan, 2 red, 3 green, 4 grey, 5 cold, 6 original */
void nebula_set_palette(nebula* n, int scheme);

/* Advance and draw one frame. t is the animation time in seconds (it is
 * multiplied by n->speed internally), so rendering is deterministic. */
void nebula_render(nebula* n, double t);

/* Internal frame size for a window of win_w x win_h pixels. width_setting is
 * the user's internal width (0 = native; may exceed the window, the frame is
 * then scaled down, which is what keeps previews showing the whole galaxy) and
 * border 0 none, 1 16:9, 2 2:3.5 cinemascope, 3 slit, as in the Haiku saver. */
void nebula_frame_size(int width_setting, int border, int win_w, int win_h,
	int* w, int* h);

/* Convert the 8 bit frame to 0x00RRGGBB pixels. pitch is in pixels. */
void nebula_blit_rgb(const nebula* n, uint32_t* dst, int pitch);

#ifdef __cplusplus
}
#endif

#endif /* NEBULA_H */
