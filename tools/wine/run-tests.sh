#!/bin/bash
# Smoke tests for nebula.scr under Wine + Xvfb.
#   usage: tools/wine/run-tests.sh path/to/nebula.scr preview_host.exe
# Needs: wine64, Xvfb, xdotool, ImageMagick (import, convert).
# preview_host.exe: x86_64-w64-mingw32-gcc -mwindows -o preview_host.exe tools/wine/preview_host.c
set -u
SCR=$(readlink -f "$1"); HOST=$(readlink -f "$2")
WORK=$(mktemp -d); trap 'kill $XVFB 2>/dev/null; rm -rf "$WORK"' EXIT
export DISPLAY=:98 WINEPREFIX=$WORK/prefix WINEDEBUG=-all WINEARCH=win64
Xvfb $DISPLAY -screen 0 1280x800x24 >/dev/null 2>&1 & XVFB=$!
sleep 2
cp "$SCR" $WORK/nebula.scr; cp "$HOST" $WORK/preview_host.exe; cd $WORK
fail=0
check() { if [ "$2" = ok ]; then echo "PASS $1"; else echo "FAIL $1"; fail=1; fi; }
# anchored so that a parent shell mentioning the command line doesn't match
running() { pgrep -f "^[^ ]*wine[^ ]* nebula.scr /s" >/dev/null; }
start_saver() { wine64 nebula.scr /s >/dev/null 2>&1 & sleep 6; }

start_saver
import -window root saver.png
[ "$(convert saver.png -format '%[fx:mean>0.002?1:0]' info:)" = 1 ] && r=ok || r=bad
check "/s draws something" $r
xdotool mousemove 600 400; sleep 1; xdotool mousemove 700 450; sleep 2
running && r=bad || r=ok; check "/s exits on mouse movement" $r
start_saver; xdotool mousemove 650 400 click 1; sleep 2
running && r=bad || r=ok; check "/s exits on click" $r
start_saver; xdotool windowfocus "$(xdotool search --name Nebula | head -1)" key a; sleep 2
running && r=bad || r=ok; check "/s exits on key" $r

wine64 preview_host.exe & sleep 4
W=$(xdotool search --name "preview host" | head -1)
import -window "$W" preview.png
[ "$(convert preview.png -format '%[fx:mean>0.0005?1:0]' info:)" = 1 ] && r=ok || r=bad
check "/p draws into the parent window" $r
wait $! ; [ $? = 0 ] && r=ok || r=bad
check "/p exits when the parent is destroyed" $r

# config dialog: blur off, next colour, OK; coordinates assume 1280x800
wine64 nebula.scr /c & sleep 5
xdotool mousemove 585 421 click 1; sleep 0.5
xdotool mousemove 690 368 click 1; sleep 0.5; xdotool mousemove 690 430 click 1; sleep 0.5
xdotool mousemove 696 529 click 1; sleep 2
reg=$(wine64 reg query 'HKCU\Software\nebula-screensaver' 2>&1)
echo "$reg" | grep -q 'blur *REG_DWORD *0x0' && echo "$reg" | grep -q 'scheme *REG_DWORD *0x3' && r=ok || r=bad
check "/c saves settings to the registry" $r

exit $fail
