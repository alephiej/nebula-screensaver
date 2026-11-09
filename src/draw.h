/*
 * Copyright 2004, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Copyright 2015, Augustin Cavalier <waddlesplash>. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Effect from corTeX / Optimum.
 */
#ifndef NEBULA_DRAW_H
#define NEBULA_DRAW_H

#include <stdint.h>

void memshset(uint8_t* dst, int center_shade, int fixed_shade, int half_length);
void mblur(uint8_t* src, int nbpixels);
void draw_stars(int star_width, uint8_t* dst, uint8_t inc);

#endif /* NEBULA_DRAW_H */
