/*
 * Copyright 2004, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Copyright 2015, Augustin Cavalier <waddlesplash>. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Effect from corTeX / Optimum.
 */

#include "nebula.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

/* Disk shading can run along a row past the frame edge (it wraps into the
 * neighbouring rows), so keep some slack around the frame. */
#define GUARD 1024

typedef float matrix[3][3];


/* xorshift32, so a seed gives the same galaxy on every platform */
static uint32_t
rnd(uint32_t* state)
{
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return *state = x;
}


static double
rnd01(uint32_t* state)
{
	return rnd(state) / 4294967296.0;
}


static inline float
ocos(const nebula* n, float a)
{
	return n->precos[(int)(a * 256 / M_PI) & 511];
}


static inline float
osin(const nebula* n, float a)
{
	return n->presin[(int)(a * 256 / M_PI) & 511];
}


static void
mulmat(matrix* a, matrix* b, matrix* c)
{
	int i, j;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			(*c)[i][j] = (*a)[i][0] * (*b)[0][j]
				+ (*a)[i][1] * (*b)[1][j]
				+ (*a)[i][2] * (*b)[2][j];
		}
	}
}


static inline void
mulvec(matrix* a, float* x, float* y, float* z)
{
	float nx = *x, ny = *y, nz = *z;

	*x = nx * (*a)[0][0] + ny * (*a)[0][1] + nz * (*a)[0][2];
	*y = nx * (*a)[1][0] + ny * (*a)[1][1] + nz * (*a)[1][2];
	*z = nx * (*a)[2][0] + ny * (*a)[2][1] + nz * (*a)[2][2];
}


static void
setrmat(float a, float b, float c, matrix* m)
{
	int i, j;
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			(*m)[i][j] = (float)(i == j);

	if (a != 0) {
		(*m)[0][0] = cos(a);	(*m)[0][1] = sin(a);
		(*m)[1][0] = sin(a);	(*m)[1][1] = -cos(a);
		return;
	}
	if (b != 0) {
		(*m)[0][0] = cos(b);	(*m)[0][2] = sin(b);
		(*m)[2][0] = sin(b);	(*m)[2][2] = -cos(b);
		return;
	}
	(*m)[1][1] = cos(c);	(*m)[1][2] = sin(c);
	(*m)[2][1] = sin(c);	(*m)[2][2] = -cos(c);
}


static void
drawshdisk(nebula* n, int x0, int y0, int r)
{
	int x = 0;
	int y;
	int ly;		/* last y */
	int delta;
	int c;		/* color at center */
	int d;		/* delta */
	const int w = n->width;
	const uint32_t h3 = (uint32_t)(n->height - 3);
	uint8_t* buf = n->frame;

#define SLIMIT 17
#define SRANGE 15

	if (r <= SLIMIT) {
		/* range checking is already (more or less) done... */
		draw_stars(w, &buf[x0 + w * y0], 10 + r * 5);
		return;
	}

	if (r < SLIMIT + SRANGE)
		r = ((r - SLIMIT) * SLIMIT) / SRANGE + 1;

	y = ly = r;
	delta = 3 - 2 * r;

	do {
		if (y != ly) {
			/* dont overlap these lines */
			c = ((r - y + 1) << 13) / r;
			d = -c / (x + 1);

			if (y == x + 1)		/* this would overlap with the next x lines */
				goto next;

			/* (uint32)(v) < M  <=>  0 <= v < M, one compare instead of two */
			if ((uint32_t)(y0 - y - 1) < h3)
				memshset(&buf[x0 + w * (y0 - y + 1)], c, d, x);

			if ((uint32_t)(y0 + y - 1) < h3)
				memshset(&buf[x0 + w * (y0 + y)], c, d, x);
		}
	next:
		c = ((r - x + 1) << 13) / r;
		d = -c / y;

		if ((uint32_t)(y0 - x - 1) < h3)
			memshset(&buf[x0 + w * (y0 - x)], c, d, y);
		if ((uint32_t)(y0 + x - 1) < h3)
			memshset(&buf[x0 + w * (y0 + x + 1)], c, d, y);

		ly = y;
		if (delta < 0)
			delta += 4 * x + 6;
		else {
			delta += 4 * (x - y) + 10;
			y--;
		}
		x++;
	} while (x < y);
}


nebula*
nebula_create(int width, int height, unsigned seed)
{
	nebula* n;
	uint32_t rs = seed ? seed : 0x9e3779b9u;
	int i;

	if (width < 16 || height < 16)
		return NULL;

	n = calloc(1, sizeof(*n));
	if (n == NULL)
		return NULL;

	n->width = width;
	n->height = height;
	n->speed = 0.4f;
	n->motion_blur = 1;

	n->storage = calloc(1, (size_t)width * height + 2 * GUARD);
	if (n->storage == NULL) {
		free(n);
		return NULL;
	}
	n->frame = n->storage + GUARD;

	for (i = 0; i < 512; i++) {
		n->precos[i] = cos(i * M_PI / 256);
		n->presin[i] = sin(i * M_PI / 256);
	}

	for (i = 0; i < NEBULA_STARS; i++) {
		double r, th, h, dth;

		r = rnd01(&rs);
		r = (1 - r) * (1 - r) + 0.05;

		if (r < 0.12)
			th = rnd01(&rs) * M_PI * 2;
		else {
			th = (rnd(&rs) & 3) * M_PI_2 + r * r * 2;
			dth = rnd01(&rs);
			dth = dth * dth * 2;
			th += dth;
		}
		n->stars[i].x = (int)(512 * r * cos(th));
		n->stars[i].z = (int)(512 * r * sin(th));
		h = (1 + cos(r * M_PI)) * 150;
		dth = rnd01(&rs);
		n->stars[i].y = (int)(h * (dth - 0.5));
		n->stars[i].r = (int)((2 - r) * 60 + 31);
	}
	/* the bright core */
	n->stars[0].x = n->stars[0].y = n->stars[0].z = 0;
	n->stars[0].r = 320;

	nebula_set_palette(n, 0);
	return n;
}


void
nebula_destroy(nebula* n)
{
	if (n == NULL)
		return;
	free(n->storage);
	free(n);
}


void
nebula_render(nebula* n, double time)
{
	int r, x, y, i;
	float rx, ry, rz;
	float oa, ob, oc;
	float t, a, b, c;
	matrix ma, mb, mc, mr;
	const int w = n->width, h = n->height;

	t = (float)(n->speed * time);

	a = 0.9f * t;
	b = t;
	c = 1.1f * t;

	setrmat(a, 0, 0, &ma);
	setrmat(0, b, 0, &mb);
	mulmat(&ma, &mb, &mc);
	setrmat(0, 0, c, &ma);
	mulmat(&ma, &mc, &mr);

	oa = 140 * osin(n, a);
	ob = 140 * ocos(n, b);
	oc = 240 * osin(n, c);

	if (n->motion_blur)
		mblur(n->frame, w * h);
	else
		memset(n->frame, 0, (size_t)w * h);

	for (i = 0; i < NEBULA_STARS; i++) {
		rx = n->stars[i].x;
		ry = n->stars[i].y;
		rz = n->stars[i].z;

		mulvec(&mr, &rx, &ry, &rz);

		rx += oa;
		ry += ob;
		rz += oc;
		rz += 300;

		if (rz > 5) {
			x = (int)(15 * rx / (rz / 5 + 1)) + w / 2;
			y = (int)(15 * ry / (rz / 5 + 1)) + h / 2;
			r = (int)(3 * n->stars[i].r / (rz / 4 + 3)) + 2;

			if (x > 5 && x < w - 6 && y > 5 && y < h - 6)
				drawshdisk(n, x, y, r);
		}
	}
}


void
nebula_frame_size(int width_setting, int border, int win_w, int win_h,
	int* w, int* h)
{
	float factor;

	*w = width_setting > 0 ? width_setting : win_w;
	factor = (float)win_w / *w;
	*h = (int)(win_h / factor + 0.5f);

	switch (border) {
		case 1: *h = (int)(*h * 0.703125 + 0.5); break;
		case 2: *h = (int)(*h * 0.534 + 0.5); break;
		case 3: *h /= 5; break;
	}
	if (*w < 16)
		*w = 16;
	if (*h < 16)
		*h = 16;
}


void
nebula_blit_rgb(const nebula* n, uint32_t* dst, int pitch)
{
	int x, y;
	for (y = 0; y < n->height; y++) {
		const uint8_t* src = n->frame + (size_t)y * n->width;
		uint32_t* out = dst + (size_t)y * pitch;
		for (x = 0; x < n->width; x++)
			out[x] = n->palette[src[x]];
	}
}


void
nebula_set_palette(nebula* n, int scheme)
{
	uint32_t* p = n->palette;
	int i;

	switch (scheme) {
		case 0:		/* yellow */
		default:
			for (i = 0; i < 30; i++)
				p[i] = (uint32_t)(uint8_t)(i * 8 / 10) << 16
					| (uint32_t)(uint8_t)(i * 6 / 10) << 8;
			for (i = 30; i < 256; i++) {
				uint8_t r = i;
				uint8_t g = i * i >> 8;
				uint8_t b = i >= 240 ? (i - 240) << 3 : 0;
				p[i] = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
			}
			break;

		case 1:		/* cyan */
			for (i = 0; i < 30; i++)
				p[i] = (uint8_t)(i * 8 / 10);
			for (i = 30; i < 256; i++) {
				uint8_t b = i;
				uint8_t g = i * i >> 8;
				uint8_t r = i >= 240 ? (i - 240) << 3 : 0;
				p[i] = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
			}
			break;

		case 2:		/* red */
			for (i = 0; i < 128; i++)
				p[i] = (uint32_t)i << 16;
			for (i = 128; i < 256; i++) {
				uint8_t r = i;
				uint8_t c = (uint8_t)((cos((i - 256) / 42.0) * 0.5 + 0.5) * 225);
				p[i] = (uint32_t)r << 16 | (uint32_t)c << 8 | c;
			}
			break;

		case 3:		/* green */
			for (i = 0; i < 30; i++)
				p[i] = (uint32_t)(uint8_t)(i * 8 / 10) << 8;
			for (i = 30; i < 256; i++) {
				uint8_t g = i;
				uint8_t r = i * i >> 8;
				uint8_t b = i >= 240 ? (i - 240) << 3 : 0;
				p[i] = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
			}
			break;

		case 4:		/* grey */
			for (i = 0; i < 256; i++) {
				uint8_t c = i * 15 / 16 + 10;
				p[i] = (uint32_t)c << 16 | (uint32_t)c << 8 | c;
			}
			break;

		case 5:		/* cold */
			for (i = 0; i < 30; i++)
				p[i] = (uint32_t)(uint8_t)(i * 8 / 10) << 16;
			for (i = 30; i < 256; i++) {
				uint8_t r = i;
				uint8_t c = (uint8_t)((cos((i - 255) / 82.0) * 0.5 + 0.5) * 255);
				p[i] = (uint32_t)r << 16 | (uint32_t)c << 8 | c;
			}
			break;

		case 6:		/* original: c is a sign-extended char, as in the Haiku
					 * version, which gives its orange tint above 127 */
			for (i = 0; i < 256; i++) {
				uint32_t c = (uint32_t)(int32_t)(int8_t)i;
				p[i] = c << 16 | c << 8;
			}
			break;
	}
}
