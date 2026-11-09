/*
 * Copyright 2004, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Copyright 2015, Augustin Cavalier <waddlesplash>. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Effect from corTeX / Optimum.
 */

#include <string.h>

#include "draw.h"


/* Hand-translated from x86 assembly.
 * dst is the *center* of the destination segment, center_shade the initial
 * color at the center (8:8 fixed point), fixed_shade the fixed point
 * increment (8:8) and half_length the length to set to the left and right. */
void
memshset(uint8_t* dst, int center_shade, int fixed_shade, int half_length)
{
	uint8_t* left;
	uint8_t* right;

	if (half_length == 0)
		return;

	left = dst - half_length;
	right = dst + half_length + 1;	/* don't overlap at middle */

	{
		int offset = -half_length - 1;	/* for in-advance incl in loop */

		do {
			int tmp;
			center_shade += fixed_shade;
			center_shade &= 0xFFFF;
			offset++;

			tmp = left[half_length] + (center_shade >> 8);
			left[half_length] = (tmp & 0xFF00) ? 255 : tmp;

			tmp = right[offset] + (center_shade >> 8);
			right[offset] = (tmp & 0xFF00) ? 255 : tmp;
		} while (--half_length != 0);
	}
}


/* The "motion blur" (actually pixel fading): v = v / 2 + v / 8, four pixels
 * at a time. memcpy keeps this free of alignment and aliasing problems. */
void
mblur(uint8_t* src, int nbpixels)
{
	int i;

	for (i = 0; i + 4 <= nbpixels; i += 4) {
		uint32_t eax, ebx;
		memcpy(&eax, src + i, 4);
		ebx = (eax >> 3) & 0x1f1f1f1fu;
		eax = ((eax >> 1) & 0x7f7f7f7fu) + ebx;
		memcpy(src + i, &eax, 4);
	}
}


/* Draw a star (5 pixels). */
void
draw_stars(int star_width, uint8_t* dst, uint8_t inc)
{
	uint8_t al, cl;

	dst[star_width] += inc;
	if (dst[star_width] < inc) {
		/* Overflowed. */
		dst[star_width] = 255;
	}

	inc >>= 1;
	cl = dst[star_width - 1];
	cl += inc;
	if (cl < inc) {
		*dst = 255;
		dst[star_width - 1] = 255;
	} else {
		al = *dst;
		al += inc;
		if (al >= inc)
			*dst = al;
		else
			*dst = 255;
		dst[star_width - 1] = cl;
	}

	al = dst[star_width * 2];
	cl = dst[star_width + 1];
	al += inc;
	cl += inc;
	if (al < inc) {
		if (cl >= inc) {
			dst[star_width * 2] = 255;
			dst[star_width + 1] = cl;
			return;
		}
	} else {
		if (cl >= inc) {
			dst[star_width * 2] = al;
			dst[star_width + 1] = cl;
			return;
		}
	}

	dst[star_width * 2] = 255;
	dst[star_width + 1] = 255;
}
