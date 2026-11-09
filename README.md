# nebula-sdl

Standalone port of the Nebula screen saver from Haiku/Vitruvian
(`src/add-ons/screen_savers/nebula`): a rotating 5000-star galaxy, rendered
entirely on the CPU into an 8 bit buffer and shaded through a 256-entry
palette. Effect from corTeX / Optimum.

## Layout

- `src/nebula.[ch]` – portable C core, no platform dependencies, no globals.
  Rendering is a pure function of time (`nebula_render(n, t)`), and the galaxy
  is reproducible from a seed.
- `src/draw.[ch]` – the old x86-assembly inner loops (disk shading, motion
  blur, stars), now alignment-safe.
- `src/main.c` – SDL2 front end (window, letterboxing, input).
- `win32/` – native Windows screen saver (`nebula.scr`), no SDL needed:
  `scr_main.c` (shell), `settings.c` (registry + config dialog), `nebula.rc`
  and `nebula.manifest` (dialog, visual styles, per-monitor DPI).
- `tools/render_ppm.c` – headless PPM renderer for testing without SDL.

## Build

    cmake -S . -B build && cmake --build build
    ./build/nebula --size 1280x720
    ./build/render_ppm out.ppm 640 480 200 0 1 0.05

Without SDL2 only the core and `render_ppm` are built. On Windows the build
also produces `nebula.scr` (MSVC or MinGW), which needs only system DLLs.
Cross-compiling from Linux:

    cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
    cmake --build build-win

## Windows screen saver

Copy `nebula.scr` to `C:\Windows\System32` (or right-click > Install) and pick
it in Settings > Personalization > Lock screen > Screen saver settings.

- `/s` runs on every monitor (topmost, hidden cursor, any key, click or
  mouse movement of more than 8 px exits, as does losing focus).
- `/p <hwnd>` renders into the preview pane of the settings dialog.
- `/c` (or no argument) shows the settings dialog: internal width, colour,
  format, motion blur, speed and frame rate. Stored under
  `HKCU\Software\nebula-screensaver`.
- Per-monitor DPI aware; each monitor gets its own galaxy.

## Keys and options

`Esc`/`Q` quit, `F` fullscreen, `C` next palette, `B` toggle blur. Run with
`--help`-style bad args for the option list (`--width`, `--scheme`,
`--no-blur`, `--speed`, `--fps`, `--border`, `--fullscreen`, `--saver`).

## Testing

`tools/wine/run-tests.sh nebula.scr preview_host.exe` runs smoke tests under
Wine + Xvfb (needs wine64, Xvfb, xdotool, ImageMagick): `/s` draws and exits on
mouse/click/key, `/p` draws into a host window and exits when it goes away,
`/c` saves to the registry. Build `preview_host.exe` from
`tools/wine/preview_host.c` with the same MinGW toolchain.

## Linux (Wayland) screen saver

The SDL build doubles as a screen saver under an idle daemon. In `--saver`
mode it does not inhibit idle, so later timeouts still fire. For sway:

    exec swayidle \
        timeout 600  'SDL_VIDEODRIVER=wayland nebula --saver' \
             resume  'pkill -x nebula' \
        timeout 1800 'swaymsg "output * power off"' \
             resume  'swaymsg "output * power on"'

Don't pass `-w` to swayidle: it then waits for the saver to exit before
handling the next timeout, so the display only powers off once you are back.

## Tested on

- Windows, on a PC with three monitors: `nebula.scr` runs one galaxy per screen.
- Wine 10.0 under Xvfb (the smoke tests in `tools/wine`).
- Linux, sway (Wayland), with swayidle as above.

## TODO

- Treat multiple monitors as a single display (one galaxy spanning all
  screens) instead of one galaxy per monitor, at least as an option.
- Per-monitor DPI on mixed-DPI setups is untested.
- The preview host in the tests is ours; the real one is the Screen Saver
  Settings dialog.
- SDL front end has no settings dialog; use command line options.
- Settings dialog has no live preview.

## Differences from the Haiku version

- Animation time is `speed * seconds`; the original mixed up operator
  precedence and effectively subtracted an unscaled start time.
- Galaxy generation uses its own xorshift RNG instead of `rand()`.
- The frame buffer has guard bytes: disk shading near the frame edges can
  run past the buffer end in the original.

## License

MIT, see `LICENSE`. Keep the original attributions.
