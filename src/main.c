/*
 * SDL2 front end for the Nebula core.
 * Distributed under the terms of the MIT License.
 *
 * Windows screen saver style switches are accepted too: /s (run), /p <hwnd>
 * (preview, ignored for now) and /c (configuration, not implemented).
 */

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nebula.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

#define IDLE_QUIT_MOTION 8		/* pixels of mouse travel that end the saver */


typedef struct {
	int width;			/* internal render width, 0 = window width */
	int scheme;
	int blur;
	float speed;
	int fps;
	int border;			/* 0 none, 1 16:9, 2 2:3.5, 3 slit */
	int fullscreen;
	int saver;			/* quit on any input */
	int win_w, win_h;
} options;


static void
usage(const char* prog)
{
	printf("usage: %s [options]\n"
		"  --fullscreen        borderless desktop fullscreen\n"
		"  --saver             exit on any key or mouse movement (implies --fullscreen)\n"
		"  --size WxH          window size (default 1280x720)\n"
		"  --width N           internal render width, 0 = native (default 640)\n"
		"  --scheme N          0 yellow, 1 cyan, 2 red, 3 green, 4 grey, 5 cold,\n"
		"                      6 original (default 0)\n"
		"  --no-blur           disable motion blur\n"
		"  --speed F           animation speed (default 0.4)\n"
		"  --fps N             frame rate limit (default 40)\n"
		"  --border N          0 none, 1 16:9, 2 2:3.5 cinemascope, 3 slit\n",
		prog);
}


static int
parse_options(int argc, char** argv, options* o)
{
	int i;

	o->width = 640;
	o->scheme = 0;
	o->blur = 1;
	o->speed = 0.4f;
	o->fps = 40;
	o->border = 0;
	o->fullscreen = 0;
	o->saver = 0;
	o->win_w = 1280;
	o->win_h = 720;

	for (i = 1; i < argc; i++) {
		const char* a = argv[i];
		const char* v = i + 1 < argc ? argv[i + 1] : NULL;

		if (strcmp(a, "--fullscreen") == 0)
			o->fullscreen = 1;
		else if (strcmp(a, "--saver") == 0 || strcasecmp(a, "/s") == 0)
			o->saver = o->fullscreen = 1;
		else if (strcmp(a, "--no-blur") == 0)
			o->blur = 0;
		else if (strcmp(a, "--size") == 0 && v) {
			if (sscanf(v, "%dx%d", &o->win_w, &o->win_h) != 2)
				return -1;
			i++;
		} else if (strcmp(a, "--width") == 0 && v)
			o->width = atoi(argv[++i]);
		else if (strcmp(a, "--scheme") == 0 && v)
			o->scheme = atoi(argv[++i]);
		else if (strcmp(a, "--speed") == 0 && v)
			o->speed = (float)atof(argv[++i]);
		else if (strcmp(a, "--fps") == 0 && v)
			o->fps = atoi(argv[++i]);
		else if (strcmp(a, "--border") == 0 && v)
			o->border = atoi(argv[++i]);
		else if (strcasecmp(a, "/p") == 0 || strncasecmp(a, "/p:", 3) == 0)
			return 1;		/* preview: nothing to do */
		else if (strncasecmp(a, "/c", 2) == 0) {
			fprintf(stderr, "no configuration dialog; use command line options\n");
			return 1;
		} else
			return -1;
	}

	if (o->fps < 1)
		o->fps = 1;
	if (o->speed < 0.01f)
		o->speed = 0.4f;
	if (o->scheme < 0 || o->scheme >= NEBULA_PALETTE_SCHEMES)
		o->scheme = 0;
	return 0;
}


int
main(int argc, char** argv)
{
	options opt;
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	nebula* neb;
	uint32_t* pixels;
	int fw, fh, win_w, win_h, running = 1, rc;
	Uint64 start, freq;
	int mouse_seen = 0;

	rc = parse_options(argc, argv, &opt);
	if (rc < 0) {
		usage(argv[0]);
		return 2;
	}
	if (rc > 0)
		return 0;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	/* As a saver we are started by the idle daemon (swayidle etc.), and
	 * must not inhibit idle ourselves, or its later timeouts (DPMS off,
	 * lock) never fire. Interactively, keep the display awake. */
	if (opt.saver)
		SDL_EnableScreenSaver();
	else
		SDL_DisableScreenSaver();
	/* Nearest-neighbour keeps the low-res look the effect was made for */
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

	window = SDL_CreateWindow("Nebula", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, opt.win_w, opt.win_h,
		SDL_WINDOW_RESIZABLE
			| (opt.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	if (window == NULL) {
		fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
		return 1;
	}
	if (opt.saver)
		SDL_ShowCursor(SDL_DISABLE);

	renderer = SDL_CreateRenderer(window, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL)
		renderer = SDL_CreateRenderer(window, -1, 0);
	if (renderer == NULL) {
		fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
		return 1;
	}

	SDL_GetWindowSize(window, &win_w, &win_h);
	nebula_frame_size(opt.width, opt.border, win_w, win_h, &fw, &fh);

	neb = nebula_create(fw, fh, (unsigned)SDL_GetTicks() | 1);
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING, fw, fh);
	pixels = malloc((size_t)fw * fh * sizeof(uint32_t));
	if (neb == NULL || texture == NULL || pixels == NULL) {
		fprintf(stderr, "initialisation failed: %s\n", SDL_GetError());
		return 1;
	}
	neb->motion_blur = opt.blur;
	neb->speed = opt.speed;
	nebula_set_palette(neb, opt.scheme);

	start = SDL_GetPerformanceCounter();
	freq = SDL_GetPerformanceFrequency();

	while (running) {
		SDL_Event ev;
		Uint32 frame_start = SDL_GetTicks();
		SDL_Rect dst;
		double t;

		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
				case SDL_QUIT:
					running = 0;
					break;
				case SDL_KEYDOWN:
					if (opt.saver || ev.key.keysym.sym == SDLK_ESCAPE
						|| ev.key.keysym.sym == SDLK_q)
						running = 0;
					else if (ev.key.keysym.sym == SDLK_f) {
						opt.fullscreen = !opt.fullscreen;
						SDL_SetWindowFullscreen(window,
							opt.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
					} else if (ev.key.keysym.sym == SDLK_c) {
						opt.scheme = (opt.scheme + 1) % NEBULA_PALETTE_SCHEMES;
						nebula_set_palette(neb, opt.scheme);
					} else if (ev.key.keysym.sym == SDLK_b)
						neb->motion_blur = !neb->motion_blur;
					break;
				case SDL_MOUSEBUTTONDOWN:
					if (opt.saver)
						running = 0;
					break;
				case SDL_MOUSEMOTION:
					/* the first event is often synthetic when the window
					 * appears, and a saver shouldn't die from a twitch */
					if (opt.saver && mouse_seen
						&& (abs(ev.motion.xrel) + abs(ev.motion.yrel)
							> IDLE_QUIT_MOTION))
						running = 0;
					mouse_seen = 1;
					break;
			}
		}

		t = (double)(SDL_GetPerformanceCounter() - start) / freq;
		nebula_render(neb, t);
		nebula_blit_rgb(neb, pixels, fw);
		SDL_UpdateTexture(texture, NULL, pixels, fw * (int)sizeof(uint32_t));

		SDL_GetWindowSize(window, &win_w, &win_h);
		/* letterbox: fit the frame in the window, keeping aspect ratio */
		{
			float s = (float)win_w / fw;
			if ((float)win_h / fh < s)
				s = (float)win_h / fh;
			dst.w = (int)(fw * s);
			dst.h = (int)(fh * s);
			dst.x = (win_w - dst.w) / 2;
			dst.y = (win_h - dst.h) / 2;
		}
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, texture, NULL, &dst);
		SDL_RenderPresent(renderer);

		{
			/* frame limiter on top of (possibly absent) vsync */
			Uint32 spent = SDL_GetTicks() - frame_start;
			Uint32 budget = 1000 / opt.fps;
			if (spent < budget)
				SDL_Delay(budget - spent);
		}
	}

	free(pixels);
	nebula_destroy(neb);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
