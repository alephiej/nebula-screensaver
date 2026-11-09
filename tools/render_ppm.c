/* Headless renderer: writes frames as PPM, to check the core without SDL.
 * usage: render_ppm out.ppm [width height frames scheme blur t_step] */

#include <stdio.h>
#include <stdlib.h>

#include "../src/nebula.h"

int
main(int argc, char** argv)
{
	int w = argc > 2 ? atoi(argv[2]) : 640;
	int h = argc > 3 ? atoi(argv[3]) : 480;
	int frames = argc > 4 ? atoi(argv[4]) : 60;
	int scheme = argc > 5 ? atoi(argv[5]) : 0;
	int blur = argc > 6 ? atoi(argv[6]) : 1;
	double step = argc > 7 ? atof(argv[7]) : 1.0 / 40;
	nebula* n;
	uint32_t* px;
	FILE* f;
	int i;

	if (argc < 2) {
		fprintf(stderr, "usage: %s out.ppm [w h frames scheme blur step]\n",
			argv[0]);
		return 1;
	}
	n = nebula_create(w, h, 1);
	px = malloc((size_t)w * h * 4);
	if (n == NULL || px == NULL)
		return 1;
	n->motion_blur = blur;
	nebula_set_palette(n, scheme);

	for (i = 0; i < frames; i++)
		nebula_render(n, i * step);
	nebula_blit_rgb(n, px, w);

	f = fopen(argv[1], "wb");
	if (f == NULL)
		return 1;
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (i = 0; i < w * h; i++) {
		fputc(px[i] >> 16 & 255, f);
		fputc(px[i] >> 8 & 255, f);
		fputc(px[i] & 255, f);
	}
	fclose(f);
	nebula_destroy(n);
	free(px);
	return 0;
}
